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
#pragma once

#if defined(USE_NV_HW_DECODER)

#include <atomic>
#include <mutex>
#include <memory>
#include <set>
#include <string>

namespace orbbec_camera {

class NvJpegDecoderManager {
 public:
  static NvJpegDecoderManager& getInstance();

  // Try to acquire a hardware decoder slot
  bool acquireDecoderSlot(const std::string& camera_name);

  // Release a hardware decoder slot
  void releaseDecoderSlot(const std::string& camera_name);

  // Check if hardware decoding is available for this camera
  bool isHardwareDecodingAvailable(const std::string& camera_name) const;

  // Get the maximum number of concurrent hardware decoders
  int getMaxHardwareDecoders() const { return max_hw_decoders_; }

  // Set the maximum number of concurrent hardware decoders (for different Jetson models)
  void setMaxHardwareDecoders(int max_decoders);

 private:
  NvJpegDecoderManager();
  ~NvJpegDecoderManager() = default;

  // Non-copyable
  NvJpegDecoderManager(const NvJpegDecoderManager&) = delete;
  NvJpegDecoderManager& operator=(const NvJpegDecoderManager&) = delete;

  mutable std::mutex mutex_;
  std::atomic<int> active_hw_decoders_{0};
  int max_hw_decoders_{1}; // Conservative default, can be adjusted based on Jetson model
  std::set<std::string> hw_decoder_cameras_;
};

} // namespace orbbec_camera

#endif // USE_NV_HW_DECODER