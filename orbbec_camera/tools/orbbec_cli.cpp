// Interactive CLI tool for Orbbec camera management
// Features: Device listing, firmware updates, preset management, device reboot
// Based on OrbbecSDK_v2 examples
// Copyright (c) Orbbec Inc.

#include <libobsensor/ObSensor.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <sstream>

// Predefined presets from OrbbecSDK_ROS2
const std::vector<std::string> PREDEFINED_PRESETS = {
    "Default",
    "Hand",
    "High Accuracy",
    "High Density",
    "Medium Density"
};

// Color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// Function prototypes
void printHeader(const std::string& title);
void printSeparator();
void listDevices(std::shared_ptr<ob::Context> context);
void listPresets(std::shared_ptr<ob::Context> context);
void updateFirmware(std::shared_ptr<ob::Context> context);
void installPreset(std::shared_ptr<ob::Context> context);
void rebootDevice(std::shared_ptr<ob::Context> context);
void showHelp();
void firmwareUpdateCallback(OBFwUpdateState state, const char* message, uint8_t percent);
std::string toLowerCase(const std::string& str);

bool g_firstCallback = true;

int main() try {
    std::cout << COLOR_BOLD << COLOR_CYAN;
    printHeader("Orbbec Camera Management CLI");
    std::cout << COLOR_RESET;
    std::cout << "Interactive tool for managing Orbbec cameras\n";
    std::cout << "Type 'help' for available commands\n";
    printSeparator();

    // Create context
    auto context = std::make_shared<ob::Context>();
    
#if defined(__linux__)
    // Use libuvc backend on Linux for better reliability
    context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    std::string command;
    
    while (true) {
        std::cout << COLOR_BOLD << COLOR_GREEN << "\norbbec> " << COLOR_RESET;
        std::getline(std::cin, command);
        
        // Trim whitespace
        command.erase(0, command.find_first_not_of(" \t\n\r"));
        command.erase(command.find_last_not_of(" \t\n\r") + 1);
        
        if (command.empty()) {
            continue;
        }
        
        std::string cmd = toLowerCase(command);
        
        if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            std::cout << "\nGoodbye! 👋\n";
            break;
        }
        else if (cmd == "help" || cmd == "h" || cmd == "?") {
            showHelp();
        }
        else if (cmd == "list" || cmd == "ls" || cmd == "l") {
            listDevices(context);
        }
        else if (cmd == "listpresets" || cmd == "lp" || cmd == "showpresets") {
            listPresets(context);
        }
        else if (cmd == "update" || cmd == "firmware" || cmd == "fw") {
            updateFirmware(context);
        }
        else if (cmd == "preset" || cmd == "presets" || cmd == "p") {
            installPreset(context);
        }
        else if (cmd == "reboot" || cmd == "restart" || cmd == "r") {
            rebootDevice(context);
        }
        else {
            std::cout << COLOR_RED << "Unknown command: " << COLOR_RESET << command << "\n";
            std::cout << "Type 'help' for available commands\n";
        }
    }
    
    return 0;
}
catch (ob::Error& e) {
    std::cerr << COLOR_RED << "Fatal error: " << COLOR_RESET << e.what() << std::endl;
    return 1;
}

void printHeader(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(58) << title << " ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
}

void printSeparator() {
    std::cout << "────────────────────────────────────────────────────────────\n";
}

std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

void showHelp() {
    std::cout << "\n" << COLOR_BOLD << "Available Commands:\n" << COLOR_RESET;
    printSeparator();
    
    std::cout << COLOR_CYAN << "  list, ls, l" << COLOR_RESET << "\n";
    std::cout << "    List all connected Orbbec cameras with details\n\n";
    
    std::cout << COLOR_CYAN << "  listpresets, lp, showpresets" << COLOR_RESET << "\n";
    std::cout << "    List available presets for a specific device\n\n";
    
    std::cout << COLOR_CYAN << "  update, firmware, fw" << COLOR_RESET << "\n";
    std::cout << "    Update camera firmware\n\n";
    
    std::cout << COLOR_CYAN << "  preset, presets, p" << COLOR_RESET << "\n";
    std::cout << "    Install camera presets\n\n";
    
    std::cout << COLOR_CYAN << "  reboot, restart, r" << COLOR_RESET << "\n";
    std::cout << "    Reboot a camera device\n\n";
    
    std::cout << COLOR_CYAN << "  help, h, ?" << COLOR_RESET << "\n";
    std::cout << "    Show this help message\n\n";
    
    std::cout << COLOR_CYAN << "  quit, exit, q" << COLOR_RESET << "\n";
    std::cout << "    Exit the program\n";
    
    printSeparator();
}

void listDevices(std::shared_ptr<ob::Context> context) {
    try {
        auto deviceList = context->queryDeviceList();
        uint32_t count = deviceList->getCount();
        
        std::cout << "\n" << COLOR_BOLD;
        printHeader("Connected Devices");
        std::cout << COLOR_RESET;
        
        if (count == 0) {
            std::cout << COLOR_YELLOW << "No Orbbec cameras found!\n" << COLOR_RESET;
            return;
        }
        
        std::cout << "Found " << COLOR_BOLD << count << COLOR_RESET << " device(s):\n\n";
        
        for (uint32_t i = 0; i < count; ++i) {
            auto device = deviceList->getDevice(i);
            auto deviceInfo = device->getDeviceInfo();
            
            std::cout << COLOR_BOLD << COLOR_BLUE << "[" << i << "] " << COLOR_RESET;
            std::cout << COLOR_BOLD << deviceInfo->getName() << COLOR_RESET << "\n";
            
            std::cout << "    " << COLOR_CYAN << "Serial Number:  " << COLOR_RESET 
                      << deviceInfo->getSerialNumber() << "\n";
            
            std::cout << "    " << COLOR_CYAN << "Firmware:       " << COLOR_RESET 
                      << deviceInfo->getFirmwareVersion() << "\n";
            
            std::cout << "    " << COLOR_CYAN << "Hardware:       " << COLOR_RESET 
                      << deviceInfo->getHardwareVersion() << "\n";
            
            std::cout << "    " << COLOR_CYAN << "Connection:     " << COLOR_RESET 
                      << deviceInfo->getConnectionType() << "\n";
            
            std::cout << "    " << COLOR_CYAN << "SDK Version:    " << COLOR_RESET 
                      << deviceInfo->getSupportedMinSdkVersion() << "\n";
            
            std::cout << "\n";
        }
        
        printSeparator();
    }
    catch (ob::Error& e) {
        std::cerr << COLOR_RED << "Error listing devices: " << COLOR_RESET 
                  << e.what() << std::endl;
    }
}

void listPresets(std::shared_ptr<ob::Context> context) {
    try {
        auto deviceList = context->queryDeviceList();
        uint32_t count = deviceList->getCount();
        
        if (count == 0) {
            std::cout << COLOR_YELLOW << "No devices found!\n" << COLOR_RESET;
            return;
        }
        
        // List devices
        std::cout << "\n" << COLOR_BOLD << "Available devices:\n" << COLOR_RESET;
        for (uint32_t i = 0; i < count; ++i) {
            auto device = deviceList->getDevice(i);
            auto deviceInfo = device->getDeviceInfo();
            std::cout << "  [" << i << "] " << deviceInfo->getName() 
                      << " (SN: " << deviceInfo->getSerialNumber() << ")\n";
        }
        
        // Select device
        std::cout << "\nEnter device number (0-" << count-1 << ") or 'c' to cancel: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "c" || input == "cancel") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        int deviceIndex = std::stoi(input);
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(count)) {
            std::cout << COLOR_RED << "Invalid device number!\n" << COLOR_RESET;
            return;
        }
        
        auto device = deviceList->getDevice(deviceIndex);
        auto deviceInfo = device->getDeviceInfo();
        
        std::cout << "\n" << COLOR_BOLD;
        printHeader("Available Presets for " + std::string(deviceInfo->getName()));
        std::cout << COLOR_RESET;
        
        std::cout << COLOR_CYAN << "Device Serial: " << COLOR_RESET 
                  << deviceInfo->getSerialNumber() << "\n\n";
        
        try {
            auto presetList = device->getAvailablePresetList();
            
            if (!presetList || presetList->getCount() == 0) {
                std::cout << COLOR_YELLOW << "No presets available for this device.\n" << COLOR_RESET;
                return;
            }
            
            std::cout << COLOR_BOLD << "Found " << presetList->getCount() 
                      << " preset(s):\n" << COLOR_RESET;
            printSeparator();
            
            for (uint32_t i = 0; i < presetList->getCount(); ++i) {
                const char* presetName = presetList->getName(i);
                std::cout << COLOR_BOLD << COLOR_BLUE << "  [" << i << "] " 
                          << COLOR_RESET << COLOR_BOLD << presetName << COLOR_RESET << "\n";
            }
            
            std::cout << "\n";
            
            // Show current preset
            try {
                const char* currentPreset = device->getCurrentPresetName();
                std::cout << COLOR_GREEN << "📌 Current active preset: " << COLOR_RESET 
                          << COLOR_BOLD << currentPreset << COLOR_RESET << "\n";
            }
            catch (ob::Error& e) {
                std::cout << COLOR_YELLOW << "⚠️  Could not get current preset: " << COLOR_RESET 
                          << e.what() << "\n";
            }
            
            printSeparator();
            
            std::cout << "\n" << COLOR_CYAN << "💡 Tip: " << COLOR_RESET 
                      << "Use the 'preset' command to install/change presets\n";
        }
        catch (ob::Error& e) {
            std::cerr << COLOR_RED << "❌ Failed to get preset list: " << COLOR_RESET 
                      << e.what() << "\n";
        }
    }
    catch (ob::Error& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
}

void updateFirmware(std::shared_ptr<ob::Context> context) {
    try {
        auto deviceList = context->queryDeviceList();
        uint32_t count = deviceList->getCount();
        
        if (count == 0) {
            std::cout << COLOR_YELLOW << "No devices found!\n" << COLOR_RESET;
            return;
        }
        
        // List devices
        std::cout << "\n" << COLOR_BOLD << "Available devices:\n" << COLOR_RESET;
        for (uint32_t i = 0; i < count; ++i) {
            auto device = deviceList->getDevice(i);
            auto deviceInfo = device->getDeviceInfo();
            std::cout << "  [" << i << "] " << deviceInfo->getName() 
                      << " (SN: " << deviceInfo->getSerialNumber() 
                      << ", FW: " << deviceInfo->getFirmwareVersion() << ")\n";
        }
        
        // Select device
        std::cout << "\nEnter device number (0-" << count-1 << ") or 'c' to cancel: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "c" || input == "cancel") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        int deviceIndex = std::stoi(input);
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(count)) {
            std::cout << COLOR_RED << "Invalid device number!\n" << COLOR_RESET;
            return;
        }
        
        auto device = deviceList->getDevice(deviceIndex);
        auto deviceInfo = device->getDeviceInfo();
        
        // Get firmware file path
        std::cout << "Enter firmware file path: ";
        std::string firmwarePath;
        std::getline(std::cin, firmwarePath);
        
        // Verify file exists
        std::ifstream file(firmwarePath, std::ios::binary);
        if (!file.good()) {
            std::cout << COLOR_RED << "Firmware file not found: " << COLOR_RESET 
                      << firmwarePath << "\n";
            return;
        }
        file.close();
        
        // Confirm update
        std::cout << "\n" << COLOR_BOLD << "Firmware Update Summary:\n" << COLOR_RESET;
        std::cout << "  Device:       " << deviceInfo->getName() << "\n";
        std::cout << "  Serial:       " << deviceInfo->getSerialNumber() << "\n";
        std::cout << "  Current FW:   " << deviceInfo->getFirmwareVersion() << "\n";
        std::cout << "  Firmware file: " << firmwarePath << "\n\n";
        
        std::cout << COLOR_YELLOW << "⚠️  WARNING: Do not disconnect the device during update!\n" << COLOR_RESET;
        std::cout << "Proceed with firmware update? (yes/no): ";
        
        std::string confirm;
        std::getline(std::cin, confirm);
        
        if (toLowerCase(confirm) != "yes" && toLowerCase(confirm) != "y") {
            std::cout << "Update cancelled.\n";
            return;
        }
        
        // Perform update
        std::cout << "\n" << COLOR_BOLD << "Starting firmware update...\n" << COLOR_RESET;
        printSeparator();
        
        g_firstCallback = true;
        device->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);
        
        std::cout << "\n\n" << COLOR_BOLD << COLOR_GREEN;
        std::cout << "✅ Firmware update completed successfully!\n" << COLOR_RESET;
        printSeparator();
        
        // Ask to reboot
        std::cout << "\nReboot device now? (yes/no): ";
        std::string rebootResponse;
        std::getline(std::cin, rebootResponse);
        
        if (toLowerCase(rebootResponse) == "yes" || toLowerCase(rebootResponse) == "y") {
            std::cout << "\n🔄 Rebooting device...\n";
            try {
                device->reboot();
                std::cout << COLOR_GREEN << "✅ Reboot command sent!\n" << COLOR_RESET;
                std::cout << "⏳ Wait ~30 seconds for device to come back online\n";
            }
            catch (ob::Error& e) {
                std::cerr << COLOR_YELLOW << "⚠️  Reboot failed: " << COLOR_RESET 
                          << e.what() << "\n";
                std::cout << "Please power cycle the device manually.\n";
            }
        }
    }
    catch (ob::Error& e) {
        std::cerr << COLOR_RED << "\n❌ Firmware update failed: " << COLOR_RESET 
                  << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
}

void installPreset(std::shared_ptr<ob::Context> context) {
    try {
        auto deviceList = context->queryDeviceList();
        uint32_t count = deviceList->getCount();
        
        if (count == 0) {
            std::cout << COLOR_YELLOW << "No devices found!\n" << COLOR_RESET;
            return;
        }
        
        // Show available presets
        std::cout << "\n" << COLOR_BOLD << "Available Presets:\n" << COLOR_RESET;
        printSeparator();
        std::cout << "  [0] " << COLOR_CYAN << "Default" << COLOR_RESET 
                  << "         - Best visual perception, overall good performance\n";
        std::cout << "  [1] " << COLOR_CYAN << "Hand" << COLOR_RESET 
                  << "            - Clear hand and finger edges for gesture recognition\n";
        std::cout << "  [2] " << COLOR_CYAN << "High Accuracy" << COLOR_RESET 
                  << "  - High confidence depth with minimal noise\n";
        std::cout << "  [3] " << COLOR_CYAN << "High Density" << COLOR_RESET 
                  << "    - Higher fill rate with more tiny objects\n";
        std::cout << "  [4] " << COLOR_CYAN << "Medium Density" << COLOR_RESET 
                  << "  - Balanced performance in fill rate and accuracy\n";
        std::cout << "  [5] " << COLOR_CYAN << "Custom" << COLOR_RESET 
                  << "         - Load from custom preset file\n";
        printSeparator();
        
        std::cout << "\nSelect preset (0-5) or 'c' to cancel: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "c" || input == "cancel") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        int presetIndex = std::stoi(input);
        if (presetIndex < 0 || presetIndex > 5) {
            std::cout << COLOR_RED << "Invalid preset number!\n" << COLOR_RESET;
            return;
        }
        
        std::string presetName;
        std::string presetFile;
        
        if (presetIndex == 5) {
            // Custom preset
            std::cout << "Enter preset file path: ";
            std::getline(std::cin, presetFile);
            
            std::ifstream file(presetFile, std::ios::binary);
            if (!file.good()) {
                std::cout << COLOR_RED << "Preset file not found: " << COLOR_RESET 
                          << presetFile << "\n";
                return;
            }
            file.close();
            presetName = "Custom";
        }
        else {
            presetName = PREDEFINED_PRESETS[presetIndex];
        }
        
        // List devices
        std::cout << "\n" << COLOR_BOLD << "Available devices:\n" << COLOR_RESET;
        for (uint32_t i = 0; i < count; ++i) {
            auto device = deviceList->getDevice(i);
            auto deviceInfo = device->getDeviceInfo();
            std::cout << "  [" << i << "] " << deviceInfo->getName() 
                      << " (SN: " << deviceInfo->getSerialNumber() << ")\n";
        }
        
        std::cout << "\nEnter device number (0-" << count-1 << ") or 'c' to cancel: ";
        std::getline(std::cin, input);
        
        if (input == "c" || input == "cancel") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        int deviceIndex = std::stoi(input);
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(count)) {
            std::cout << COLOR_RED << "Invalid device number!\n" << COLOR_RESET;
            return;
        }
        
        auto device = deviceList->getDevice(deviceIndex);
        auto deviceInfo = device->getDeviceInfo();
        
        std::cout << "\n" << COLOR_BOLD << "Preset Installation Summary:\n" << COLOR_RESET;
        std::cout << "  Device: " << deviceInfo->getName() << "\n";
        std::cout << "  Serial: " << deviceInfo->getSerialNumber() << "\n";
        std::cout << "  Preset: " << presetName << "\n";
        if (!presetFile.empty()) {
            std::cout << "  File:   " << presetFile << "\n";
        }
        
        std::cout << "\nProceed? (yes/no): ";
        std::string confirm;
        std::getline(std::cin, confirm);
        
        if (toLowerCase(confirm) != "yes" && toLowerCase(confirm) != "y") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        std::cout << "\n🔄 Installing preset...\n";
        
        // Apply preset using device preset API
        try {
            if (presetIndex == 5 && !presetFile.empty()) {
                // Load custom preset from file
                device->loadPresetFromJsonFile(presetFile.c_str());
            }
            else {
                // Use predefined preset
                auto presetList = device->getAvailablePresetList();
                if (presetList && presetIndex < presetList->getCount()) {
                    device->loadPreset(presetList->getName(presetIndex));
                }
            }
            
            std::cout << COLOR_GREEN << "✅ Preset installed successfully!\n" << COLOR_RESET;
            std::cout << "\n💡 The preset will take effect on the next stream start\n";
        }
        catch (ob::Error& e) {
            std::cerr << COLOR_RED << "❌ Failed to install preset: " << COLOR_RESET 
                      << e.what() << "\n";
            std::cout << "\n💡 Note: Preset installation may require the camera to be streaming\n";
            std::cout << "   or may only work through the ROS2 launch parameters.\n";
        }
    }
    catch (ob::Error& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
}

void rebootDevice(std::shared_ptr<ob::Context> context) {
    try {
        auto deviceList = context->queryDeviceList();
        uint32_t count = deviceList->getCount();
        
        if (count == 0) {
            std::cout << COLOR_YELLOW << "No devices found!\n" << COLOR_RESET;
            return;
        }
        
        // List devices
        std::cout << "\n" << COLOR_BOLD << "Available devices:\n" << COLOR_RESET;
        for (uint32_t i = 0; i < count; ++i) {
            auto device = deviceList->getDevice(i);
            auto deviceInfo = device->getDeviceInfo();
            std::cout << "  [" << i << "] " << deviceInfo->getName() 
                      << " (SN: " << deviceInfo->getSerialNumber() << ")\n";
        }
        
        std::cout << "\nEnter device number (0-" << count-1 << ") or 'c' to cancel: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "c" || input == "cancel") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        int deviceIndex = std::stoi(input);
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(count)) {
            std::cout << COLOR_RED << "Invalid device number!\n" << COLOR_RESET;
            return;
        }
        
        auto device = deviceList->getDevice(deviceIndex);
        auto deviceInfo = device->getDeviceInfo();
        
        std::cout << "\nReboot device '" << deviceInfo->getName() 
                  << "' (SN: " << deviceInfo->getSerialNumber() << ")? (yes/no): ";
        
        std::string confirm;
        std::getline(std::cin, confirm);
        
        if (toLowerCase(confirm) != "yes" && toLowerCase(confirm) != "y") {
            std::cout << "Cancelled.\n";
            return;
        }
        
        std::cout << "\n🔄 Rebooting device...\n";
        device->reboot();
        
        std::cout << COLOR_GREEN << "✅ Reboot command sent successfully!\n" << COLOR_RESET;
        std::cout << "⏳ Device will reboot in a few seconds...\n";
        std::cout << "\n💡 Wait ~30 seconds for device to come back online\n";
    }
    catch (ob::Error& e) {
        std::cerr << COLOR_RED << "Reboot failed: " << COLOR_RESET 
                  << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << COLOR_RED << "Error: " << COLOR_RESET << e.what() << std::endl;
    }
}

void firmwareUpdateCallback(OBFwUpdateState state, const char* message, uint8_t percent) {
    if (g_firstCallback) {
        g_firstCallback = false;
    }
    else {
        std::cout << "\033[3F";  // Move cursor up 3 lines
    }
    
    std::cout << "\033[K";  // Clear line
    std::cout << "Progress: " << COLOR_BOLD << static_cast<uint32_t>(percent) << "%" 
              << COLOR_RESET << std::endl;
    
    std::cout << "\033[K";
    std::cout << "Status  : ";
    
    switch (state) {
    case STAT_VERIFY_SUCCESS:
        std::cout << COLOR_GREEN << "✅ Image file verification success" << COLOR_RESET;
        break;
    case STAT_FILE_TRANSFER:
        std::cout << COLOR_CYAN << "📤 File transfer in progress" << COLOR_RESET;
        break;
    case STAT_DONE:
    case STAT_DONE_WITH_DUPLICATES:
        std::cout << COLOR_GREEN << "✅ Update completed" << COLOR_RESET;
        break;
    case STAT_IN_PROGRESS:
        std::cout << COLOR_YELLOW << "⚙️  Upgrade in progress" << COLOR_RESET;
        break;
    case STAT_START:
        std::cout << COLOR_BLUE << "🔄 Starting upgrade" << COLOR_RESET;
        break;
    case STAT_VERIFY_IMAGE:
        std::cout << COLOR_CYAN << "🔍 Verifying image file" << COLOR_RESET;
        break;
    default:
        if (state < 0) {
            std::cout << COLOR_RED << "❌ Error: " << message << COLOR_RESET;
        }
        else {
            std::cout << COLOR_YELLOW << "❓ Unknown status" << COLOR_RESET;
        }
        break;
    }
    
    std::cout << std::endl;
    std::cout << "\033[K";
    std::cout << "Message : " << message << std::endl << std::flush;
}
