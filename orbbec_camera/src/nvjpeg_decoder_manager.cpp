/*******************************************************************************
* Copyright (c) 2023 Orbbec 3D Technology, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#if defined(USE_NV_HW_DECODER)
#include "orbbec_camera/nvjpeg_decoder_manager.h"
#include <rclcpp/rclcpp.hpp>
#include <fstream>
#include <sstream>

namespace orbbec_camera {

NvJpegDecoderManager& NvJpegDecoderManager::getInstance() {
  static NvJpegDecoderManager instance;
  return instance;
}

NvJpegDecoderManager::NvJpegDecoderManager() {
  // Auto-detect Jetson model and set appropriate limits
  std::ifstream devicetree("/proc/device-tree/model");
  if (devicetree.is_open()) {
    std::string model;
    std::getline(devicetree, model);
    devicetree.close();

    // Set decoder limits based on Jetson model
    if (model.find("Orin") != std::string::npos) {
      max_hw_decoders_ = 2; // Jetson Orin supports 2 NVDEC engines
    } else if (model.find("Xavier") != std::string::npos) {
      max_hw_decoders_ = 2; // Jetson Xavier supports 2 NVDEC engines  
    } else if (model.find("Nano") != std::string::npos || 
               model.find("TX2") != std::string::npos) {
      max_hw_decoders_ = 1; // Jetson Nano/TX2 supports 1 NVDEC engine
    } else {
      max_hw_decoders_ = 1; // Conservative default
    }

    RCLCPP_INFO_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                       "Detected Jetson model: " << model << 
                       ", max hardware decoders: " << max_hw_decoders_);
  } else {
    RCLCPP_WARN_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                       "Could not detect Jetson model, using conservative limit: " << max_hw_decoders_);
  }
}

bool NvJpegDecoderManager::acquireDecoderSlot(const std::string& camera_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if this camera already has a slot
  if (hw_decoder_cameras_.find(camera_name) != hw_decoder_cameras_.end()) {
    return true;
  }

  // Check if we can allocate a new slot
  if (active_hw_decoders_.load() < max_hw_decoders_) {
    hw_decoder_cameras_.insert(camera_name);
    active_hw_decoders_++;
    RCLCPP_INFO_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                       "Allocated hardware decoder slot for camera: " << camera_name << 
                       " (" << active_hw_decoders_.load() << "/" << max_hw_decoders_ << ")");
    return true;
  }

  RCLCPP_WARN_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                     "No hardware decoder slots available for camera: " << camera_name << 
                     ", falling back to software decoding (" << active_hw_decoders_.load() << 
                     "/" << max_hw_decoders_ << " slots used)");
  return false;
}

void NvJpegDecoderManager::releaseDecoderSlot(const std::string& camera_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = hw_decoder_cameras_.find(camera_name);
  if (it != hw_decoder_cameras_.end()) {
    hw_decoder_cameras_.erase(it);
    active_hw_decoders_--;
    RCLCPP_INFO_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                       "Released hardware decoder slot for camera: " << camera_name << 
                       " (" << active_hw_decoders_.load() << "/" << max_hw_decoders_ << ")");
  }
}

bool NvJpegDecoderManager::isHardwareDecodingAvailable(const std::string& camera_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return hw_decoder_cameras_.find(camera_name) != hw_decoder_cameras_.end();
}

void NvJpegDecoderManager::setMaxHardwareDecoders(int max_decoders) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_hw_decoders_ = max_decoders;
  RCLCPP_INFO_STREAM(rclcpp::get_logger("nvjpeg_decoder_manager"), 
                     "Set max hardware decoders to: " << max_hw_decoders_);
}

} // namespace orbbec_camera

#endif // USE_NV_HW_DECODER