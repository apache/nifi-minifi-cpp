/**
 * @file GetUSBCamera.h
 * GetUSBCamera class declaration
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "libuvc/libuvc.h"

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class GetUSBCamera : public core::Processor {
 public:
  explicit GetUSBCamera(std::string name, const utils::Identifier &uuid = {})
      : core::Processor(std::move(name), uuid) {
    png_write_mtx_ = std::make_shared<std::mutex>();
    dev_access_mtx_ = std::make_shared<std::recursive_mutex>();
  }

  ~GetUSBCamera() override {
    // We cannot interrupt the PNG write process
    std::lock_guard<std::mutex> lock(*png_write_mtx_);
    cleanupUvc();
  }

  void notifyStop() override {
    // We cannot interrupt the PNG write process
    std::lock_guard<std::mutex> lock(*png_write_mtx_);
    cleanupUvc();
  }

  EXTENSIONAPI static constexpr const char* Description = "Gets images from USB Video Class (UVC)-compatible devices. "
      "Outputs one flow file per frame at the rate specified by the FPS property in the format specified by the Format property.";

  EXTENSIONAPI static const core::Property FPS;
  EXTENSIONAPI static const core::Property Width;
  EXTENSIONAPI static const core::Property Height;
  EXTENSIONAPI static const core::Property Format;
  EXTENSIONAPI static const core::Property VendorID;
  EXTENSIONAPI static const core::Property ProductID;
  EXTENSIONAPI static const core::Property SerialNo;
  static auto properties() {
    return std::array{
      FPS,
      Width,
      Height,
      Format,
      VendorID,
      ProductID,
      SerialNo
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context,
                  core::ProcessSessionFactory *session_factory) override;
  void onTrigger(core::ProcessContext *context,
                 core::ProcessSession *session) override;
  void initialize() override;

  typedef struct {
    core::ProcessContext *context;
    core::ProcessSessionFactory *session_factory;
    std::shared_ptr<core::logging::Logger> logger;
    std::shared_ptr<std::mutex> png_write_mtx;
    std::shared_ptr<std::recursive_mutex> dev_access_mtx;
    std::string format;
    uvc_frame_t *frame_buffer;
    uint16_t device_width;
    uint16_t device_height;
    uint32_t device_fps;
    double target_fps;
    std::chrono::steady_clock::time_point last_frame_time;
  } CallbackData;

  static void onFrame(uvc_frame_t *frame, void *ptr);

  // Write callback for storing camera capture data in PNG format
  class PNGWriteCallback {
   public:
    PNGWriteCallback(std::shared_ptr<std::mutex> write_mtx, uvc_frame_t *frame, uint32_t width, uint32_t height);
    int64_t operator()(const std::shared_ptr<io::OutputStream>& stream);

   private:
    std::shared_ptr<std::mutex> png_write_mtx_;
    uvc_frame_t *frame_;
    const uint32_t width_;
    const uint32_t height_;
    std::vector<uint8_t> png_output_buf_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PNGWriteCallback>::getLogger();
  };

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetUSBCamera>::getLogger();
  static std::shared_ptr<utils::IdGenerator> id_generator_;

  std::shared_ptr<std::thread> camera_thread_;
  CallbackData cb_data_;

  std::shared_ptr<std::mutex> png_write_mtx_;
  std::shared_ptr<std::recursive_mutex> dev_access_mtx_;

  uvc_frame_t *frame_buffer_ = nullptr;
  uvc_context_t *ctx_ = nullptr;
  uvc_device_t *dev_ = nullptr;
  uvc_device_handle_t *devh_ = nullptr;

  void cleanupUvc();
};

}  // namespace org::apache::nifi::minifi::processors
