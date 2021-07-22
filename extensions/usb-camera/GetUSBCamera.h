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
#include <vector>

#include "libuvc/libuvc.h"

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class GetUSBCamera : public core::Processor {
 public:
  explicit GetUSBCamera(const std::string &name, const utils::Identifier &uuid = {})
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<GetUSBCamera>::getLogger()) {
    png_write_mtx_ = std::make_shared<std::mutex>();
    dev_access_mtx_ = std::make_shared<std::recursive_mutex>();
  }

  virtual ~GetUSBCamera() {
    // We cannot interrupt the PNG write process
    std::lock_guard<std::mutex> lock(*png_write_mtx_);
    cleanupUvc();
  }

  void notifyStop() override {
    // We cannot interrupt the PNG write process
    std::lock_guard<std::mutex> lock(*png_write_mtx_);
    cleanupUvc();
  }

  static core::Property FPS;
  static core::Property Width;
  static core::Property Height;
  static core::Property Format;
  static core::Property VendorID;
  static core::Property ProductID;
  static core::Property SerialNo;

  static core::Relationship Success;
  static core::Relationship Failure;

  void onSchedule(core::ProcessContext *context,
                  core::ProcessSessionFactory *session_factory) override;
  void onTrigger(core::ProcessContext *context,
                 core::ProcessSession *session) override;
  void initialize() override;

  typedef struct {
    core::ProcessContext *context;
    core::ProcessSessionFactory *session_factory;
    std::shared_ptr<logging::Logger> logger;
    std::shared_ptr<std::mutex> png_write_mtx;
    std::shared_ptr<std::recursive_mutex> dev_access_mtx;
    std::string format;
    uvc_frame_t *frame_buffer;
    uint16_t device_width;
    uint16_t device_height;
    uint32_t device_fps;
    double target_fps;
    std::chrono::milliseconds last_frame_time;
  } CallbackData;

  static void onFrame(uvc_frame_t *frame, void *ptr);

  // Write callback for storing camera capture data in PNG format
  class PNGWriteCallback : public OutputStreamCallback {
   public:
    PNGWriteCallback(std::shared_ptr<std::mutex> write_mtx, uvc_frame_t *frame, uint32_t width, uint32_t height);
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<std::mutex> png_write_mtx_;
    uvc_frame_t *frame_;
    const uint32_t width_;
    const uint32_t height_;
    std::vector<uint8_t> png_output_buf_;
    std::shared_ptr<logging::Logger> logger_;
  };

  // Write callback for storing camera capture data as a raw RGB pixel buffer
  class RawWriteCallback : public OutputStreamCallback {
   public:
    explicit RawWriteCallback(uvc_frame_t *frame);
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    uvc_frame_t *frame_;
    std::shared_ptr<logging::Logger> logger_;
  };

 private:
  std::shared_ptr<logging::Logger> logger_;
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
