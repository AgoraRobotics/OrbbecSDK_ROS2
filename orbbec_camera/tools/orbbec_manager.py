#!/usr/bin/env python3
"""
OrbbecSDK Camera Management CLI Tool

A comprehensive command-line interface for managing multiple Orbbec cameras,
including firmware updates and preset management.

Copyright (c) 2024 Agora Robotics
Licensed under the Apache License, Version 2.0
"""

import argparse
import json
import logging
import os
import sys
import subprocess
import time
import threading
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
from concurrent.futures import ThreadPoolExecutor, as_completed

try:
    import rclpy
    from rclpy.node import Node
    from rclpy.executors import SingleThreadedExecutor
    from std_srvs.srv import Empty, SetBool
    from orbbec_camera_msgs.srv import GetDeviceInfo, GetString, SetString, SetInt32
    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False


@dataclass
class CameraDevice:
    """Represents an Orbbec camera device"""
    name: str
    pid: str
    serial: str
    connection_type: str
    usb_port: Optional[str] = None
    ip_address: Optional[str] = None
    namespace: str = "camera"

    def __str__(self):
        if self.connection_type == "Ethernet":
            return f"{self.name} (SN: {self.serial}, IP: {self.ip_address})"
        else:
            return f"{self.name} (SN: {self.serial}, USB: {self.usb_port})"


class OrbbecDeviceManager:
    """Core device management functionality"""

    def __init__(self):
        self.logger = logging.getLogger('OrbbecManager')
        self.devices: List[CameraDevice] = []
        self.ros_node: Optional[Node] = None

    def discover_devices(self) -> List[CameraDevice]:
        """Discover connected Orbbec cameras"""
        self.logger.info("Discovering connected devices...")

        try:
            # Use the existing list_devices_node tool
            result = subprocess.run(
                ['ros2', 'run', 'orbbec_camera', 'list_devices_node'],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                self.logger.error(f"Failed to list devices: {result.stderr}")
                return []

            devices = self._parse_device_output(result.stdout)
            self.devices = devices
            self.logger.info(f"Found {len(devices)} devices")
            return devices

        except subprocess.TimeoutExpired:
            self.logger.error("Device discovery timed out")
            return []
        except FileNotFoundError:
            self.logger.error("ROS2 or orbbec_camera package not found. Make sure the workspace is sourced.")
            return []

    def _parse_device_output(self, output: str) -> List[CameraDevice]:
        """Parse the output from list_devices_node"""
        devices = []
        lines = output.strip().split('\n')

        current_device = None
        for line in lines:
            line = line.strip()
            if not line:
                continue

            # Look for device info lines
            if '- Name:' in line:
                # Parse: - Name: Gemini 335, PID: 0x0669, SN/ID: CX9D5B600ED, Connection: USB
                parts = line.split(', ')
                if len(parts) >= 4:
                    name = parts[0].split('Name: ')[1]
                    pid = parts[1].split('PID: ')[1]
                    serial = parts[2].split('SN/ID: ')[1]
                    connection = parts[3].split('Connection: ')[1]

                    current_device = CameraDevice(
                        name=name,
                        pid=pid,
                        serial=serial,
                        connection_type=connection
                    )
                    devices.append(current_device)

            elif current_device and 'usb port:' in line:
                current_device.usb_port = line.split('usb port: ')[1]

            elif current_device and 'ip address:' in line:
                current_device.ip_address = line.split('ip address: ')[1]

        return devices

    def get_device_by_serial(self, serial: str) -> Optional[CameraDevice]:
        """Get device by serial number"""
        for device in self.devices:
            if device.serial == serial:
                return device
        return None

    def get_devices_by_serials(self, serials: List[str]) -> List[CameraDevice]:
        """Get multiple devices by serial numbers"""
        devices = []
        for serial in serials:
            device = self.get_device_by_serial(serial)
            if device:
                devices.append(device)
            else:
                self.logger.warning(f"Device with serial {serial} not found")
        return devices


class FirmwareManager:
    """Handles firmware update operations"""

    def __init__(self, device_manager: OrbbecDeviceManager):
        self.device_manager = device_manager
        self.logger = logging.getLogger('FirmwareManager')

    def update_firmware(self, devices: List[CameraDevice], firmware_path: str, parallel: bool = False) -> bool:
        """Update firmware for specified devices"""
        if not os.path.exists(firmware_path):
            self.logger.error(f"Firmware file not found: {firmware_path}")
            return False

        self.logger.info(f"Starting firmware update for {len(devices)} devices")
        self.logger.info(f"Firmware file: {firmware_path}")

        if parallel and len(devices) > 1:
            return self._update_firmware_parallel(devices, firmware_path)
        else:
            return self._update_firmware_sequential(devices, firmware_path)

    def _update_firmware_sequential(self, devices: List[CameraDevice], firmware_path: str) -> bool:
        """Update firmware one device at a time"""
        success_count = 0

        for i, device in enumerate(devices, 1):
            self.logger.info(f"[{i}/{len(devices)}] Updating firmware for {device}")
            if self._update_single_device(device, firmware_path):
                success_count += 1
                self.logger.info(f"✓ Successfully updated {device.serial}")
            else:
                self.logger.error(f"✗ Failed to update {device.serial}")

        self.logger.info(f"Firmware update complete: {success_count}/{len(devices)} successful")
        return success_count == len(devices)

    def _update_firmware_parallel(self, devices: List[CameraDevice], firmware_path: str) -> bool:
        """Update firmware for multiple devices in parallel"""
        self.logger.info("Running parallel firmware updates...")
        success_count = 0

        with ThreadPoolExecutor(max_workers=min(len(devices), 4)) as executor:
            futures = {
                executor.submit(self._update_single_device, device, firmware_path): device
                for device in devices
            }

            for future in as_completed(futures):
                device = futures[future]
                try:
                    if future.result():
                        success_count += 1
                        self.logger.info(f"✓ Successfully updated {device.serial}")
                    else:
                        self.logger.error(f"✗ Failed to update {device.serial}")
                except Exception as e:
                    self.logger.error(f"✗ Exception updating {device.serial}: {e}")

        self.logger.info(f"Parallel firmware update complete: {success_count}/{len(devices)} successful")
        return success_count == len(devices)

    def _update_single_device(self, device: CameraDevice, firmware_path: str) -> bool:
        """Update firmware for a single device"""
        try:
            # Launch camera node with firmware update parameter
            launch_args = [
                'ros2', 'launch', 'orbbec_camera', 'gemini_330_series.launch.py',
                f'upgrade_firmware:={firmware_path}',
                f'serial_number:={device.serial}',
                f'camera_name:={device.namespace}_{device.serial}'
            ]

            # If USB device, add usb_port
            if device.usb_port:
                launch_args.append(f'usb_port:={device.usb_port}')

            self.logger.debug(f"Running: {' '.join(launch_args)}")

            # Run the launch command and wait for completion
            process = subprocess.Popen(
                launch_args,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            # Monitor the process with timeout
            timeout = 300  # 5 minutes timeout for firmware update
            start_time = time.time()

            while process.poll() is None:
                if time.time() - start_time > timeout:
                    process.terminate()
                    self.logger.error(f"Firmware update timed out for {device.serial}")
                    return False
                time.sleep(1)

            # Check if the process completed successfully
            stdout, stderr = process.communicate()

            if process.returncode == 0:
                self.logger.debug(f"Firmware update completed for {device.serial}")
                return True
            else:
                self.logger.error(f"Firmware update failed for {device.serial}: {stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Exception during firmware update for {device.serial}: {e}")
            return False


class PresetManager:
    """Handles camera preset operations"""

    PREDEFINED_PRESETS = {
        'Default': 'Best visual perception, overall good performance',
        'Hand': 'Clear hand and finger edges for gesture recognition',
        'High Accuracy': 'High confidence depth with minimal noise',
        'High Density': 'Higher fill rate with more tiny objects',
        'Medium Density': 'Balanced performance in fill rate and accuracy'
    }

    def __init__(self, device_manager: OrbbecDeviceManager):
        self.device_manager = device_manager
        self.logger = logging.getLogger('PresetManager')

    def list_presets(self) -> Dict[str, str]:
        """List available predefined presets"""
        return self.PREDEFINED_PRESETS

    def apply_preset(self, devices: List[CameraDevice], preset_name: str, preset_file: Optional[str] = None) -> bool:
        """Apply preset to specified devices"""
        if preset_name not in self.PREDEFINED_PRESETS and not preset_file:
            self.logger.error(f"Unknown preset: {preset_name}. Use --preset-file for custom presets.")
            return False

        if preset_file and not os.path.exists(preset_file):
            self.logger.error(f"Preset file not found: {preset_file}")
            return False

        self.logger.info(f"Applying preset '{preset_name}' to {len(devices)} devices")

        success_count = 0
        for device in devices:
            if self._apply_preset_to_device(device, preset_name, preset_file):
                success_count += 1
                self.logger.info(f"✓ Applied preset to {device.serial}")
            else:
                self.logger.error(f"✗ Failed to apply preset to {device.serial}")

        self.logger.info(f"Preset application complete: {success_count}/{len(devices)} successful")
        return success_count == len(devices)

    def _apply_preset_to_device(self, device: CameraDevice, preset_name: str, preset_file: Optional[str] = None) -> bool:
        """Apply preset to a single device"""
        try:
            launch_args = [
                'ros2', 'launch', 'orbbec_camera', 'gemini_330_series.launch.py',
                f'device_preset:={preset_name}',
                f'serial_number:={device.serial}',
                f'camera_name:={device.namespace}_{device.serial}'
            ]

            if preset_file:
                launch_args.append(f'preset_firmware_path:={preset_file}')

            if device.usb_port:
                launch_args.append(f'usb_port:={device.usb_port}')

            # Launch the camera with new preset
            process = subprocess.Popen(launch_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            # Wait a moment for the camera to initialize with new settings
            time.sleep(5)

            # Terminate the launch process (we just wanted to apply the preset)
            process.terminate()
            process.wait()

            return True

        except Exception as e:
            self.logger.error(f"Exception applying preset to {device.serial}: {e}")
            return False


class OrbbecManagerCLI:
    """Command Line Interface for Orbbec Camera Management"""

    def __init__(self):
        self.setup_logging()
        self.device_manager = OrbbecDeviceManager()
        self.firmware_manager = FirmwareManager(self.device_manager)
        self.preset_manager = PresetManager(self.device_manager)

    def setup_logging(self):
        """Setup logging configuration"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.StreamHandler(sys.stdout),
                logging.FileHandler('orbbec_manager.log')
            ]
        )
        self.logger = logging.getLogger('OrbbecManagerCLI')

    def parse_args(self):
        """Parse command line arguments"""
        parser = argparse.ArgumentParser(
            description='Orbbec Camera Management Tool',
            epilog='Examples:\n'
                   '  %(prog)s list\n'
                   '  %(prog)s update-firmware --file firmware.bin --all\n'
                   '  %(prog)s set-preset --preset "High Accuracy" --serial SN123,SN456\n'
                   '  %(prog)s interactive',
            formatter_class=argparse.RawDescriptionHelpFormatter
        )

        parser.add_argument('-v', '--verbose', action='store_true',
                          help='Enable verbose logging')

        subparsers = parser.add_subparsers(dest='command', help='Available commands')

        # List command
        list_parser = subparsers.add_parser('list', help='List connected cameras')
        list_parser.add_argument('--json', action='store_true', help='Output in JSON format')

        # Update firmware command
        fw_parser = subparsers.add_parser('update-firmware', help='Update camera firmware')
        fw_parser.add_argument('--file', required=True, help='Firmware file path')
        fw_target = fw_parser.add_mutually_exclusive_group(required=True)
        fw_target.add_argument('--all', action='store_true', help='Update all cameras')
        fw_target.add_argument('--serial', help='Comma-separated list of serial numbers')
        fw_parser.add_argument('--parallel', action='store_true', help='Update cameras in parallel')

        # Set preset command
        preset_parser = subparsers.add_parser('set-preset', help='Apply camera presets')
        preset_parser.add_argument('--preset', help='Preset name (Default, Hand, High Accuracy, etc.)')
        preset_parser.add_argument('--preset-file', help='Custom preset firmware file')
        preset_target = preset_parser.add_mutually_exclusive_group(required=True)
        preset_target.add_argument('--all', action='store_true', help='Apply to all cameras')
        preset_target.add_argument('--serial', help='Comma-separated list of serial numbers')

        # List presets command
        subparsers.add_parser('list-presets', help='List available presets')

        # Interactive mode
        subparsers.add_parser('interactive', help='Interactive mode')

        return parser.parse_args()

    def run(self):
        """Main entry point"""
        args = self.parse_args()

        if args.verbose:
            logging.getLogger().setLevel(logging.DEBUG)

        if not args.command:
            self.logger.error("No command specified. Use -h for help.")
            return 1

        try:
            if args.command == 'list':
                return self.cmd_list_devices(args)
            elif args.command == 'update-firmware':
                return self.cmd_update_firmware(args)
            elif args.command == 'set-preset':
                return self.cmd_set_preset(args)
            elif args.command == 'list-presets':
                return self.cmd_list_presets(args)
            elif args.command == 'interactive':
                return self.cmd_interactive()
            else:
                self.logger.error(f"Unknown command: {args.command}")
                return 1

        except KeyboardInterrupt:
            self.logger.info("Operation cancelled by user")
            return 1
        except Exception as e:
            self.logger.error(f"Unexpected error: {e}")
            return 1

    def cmd_list_devices(self, args):
        """List connected devices command"""
        devices = self.device_manager.discover_devices()

        if not devices:
            print("No Orbbec cameras found")
            return 0

        if args.json:
            device_list = []
            for device in devices:
                device_dict = {
                    'name': device.name,
                    'pid': device.pid,
                    'serial': device.serial,
                    'connection_type': device.connection_type,
                    'usb_port': device.usb_port,
                    'ip_address': device.ip_address
                }
                device_list.append(device_dict)
            print(json.dumps(device_list, indent=2))
        else:
            print(f"\nFound {len(devices)} Orbbec camera(s):")
            print("-" * 60)
            for i, device in enumerate(devices, 1):
                print(f"{i}. {device}")
                print(f"   PID: {device.pid}")
                print(f"   Connection: {device.connection_type}")
                if device.usb_port:
                    print(f"   USB Port: {device.usb_port}")
                if device.ip_address:
                    print(f"   IP Address: {device.ip_address}")
                print()

        return 0

    def cmd_update_firmware(self, args):
        """Update firmware command"""
        devices = self.device_manager.discover_devices()
        if not devices:
            self.logger.error("No devices found")
            return 1

        if args.all:
            target_devices = devices
        else:
            serials = [s.strip() for s in args.serial.split(',')]
            target_devices = self.device_manager.get_devices_by_serials(serials)
            if not target_devices:
                self.logger.error("No matching devices found")
                return 1

        success = self.firmware_manager.update_firmware(target_devices, args.file, args.parallel)
        return 0 if success else 1

    def cmd_set_preset(self, args):
        """Set preset command"""
        devices = self.device_manager.discover_devices()
        if not devices:
            self.logger.error("No devices found")
            return 1

        if args.all:
            target_devices = devices
        else:
            serials = [s.strip() for s in args.serial.split(',')]
            target_devices = self.device_manager.get_devices_by_serials(serials)
            if not target_devices:
                self.logger.error("No matching devices found")
                return 1

        preset_name = args.preset or "Custom"
        success = self.preset_manager.apply_preset(target_devices, preset_name, args.preset_file)
        return 0 if success else 1

    def cmd_list_presets(self, args):
        """List presets command"""
        presets = self.preset_manager.list_presets()
        print("\nAvailable Presets:")
        print("-" * 50)
        for name, description in presets.items():
            print(f"• {name}")
            print(f"  {description}")
            print()
        return 0

    def cmd_interactive(self):
        """Interactive mode"""
        print("\n=== Orbbec Camera Manager - Interactive Mode ===")
        print("Type 'help' for available commands or 'quit' to exit")

        while True:
            try:
                command = input("\norbbec_manager> ").strip()
                if not command:
                    continue

                if command.lower() in ['quit', 'exit']:
                    print("Goodbye!")
                    break
                elif command.lower() == 'help':
                    self._print_interactive_help()
                elif command.lower() == 'list':
                    self.device_manager.discover_devices()
                    self.cmd_list_devices(argparse.Namespace(json=False))
                elif command.lower() == 'presets':
                    self.cmd_list_presets(None)
                else:
                    print(f"Unknown command: {command}")
                    print("Type 'help' for available commands")

            except KeyboardInterrupt:
                print("\nUse 'quit' to exit")
            except EOFError:
                print("\nGoodbye!")
                break

        return 0

    def _print_interactive_help(self):
        """Print interactive mode help"""
        print("""
Available commands:
  list      - List connected cameras
  presets   - List available presets
  help      - Show this help message
  quit/exit - Exit interactive mode

For advanced operations like firmware updates, use the command-line interface:
  ./orbbec_manager.py update-firmware --file firmware.bin --all
  ./orbbec_manager.py set-preset --preset "High Accuracy" --all
""")


def main():
    """Main entry point"""
    if not ROS2_AVAILABLE:
        print("Error: ROS2 Python libraries not available. Please install rclpy and orbbec_camera_msgs.")
        return 1

    cli = OrbbecManagerCLI()
    return cli.run()


if __name__ == '__main__':
    sys.exit(main())