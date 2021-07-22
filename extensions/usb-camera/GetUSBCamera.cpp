/**
 * @file GetUSBCamera.cpp
 * GetUSBCamera class implementation
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
#include "GetUSBCamera.h"

#include <png.h>

#include <utility>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include "utils/gsl.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property GetUSBCamera::FPS(  // NOLINT
    "FPS", "Frames per second to capture from USB camera", "1");
core::Property GetUSBCamera::Width(  // NOLINT
    "Width", "Target width of image to capture from USB camera", "");
core::Property GetUSBCamera::Height(  // NOLINT
    "Height", "Target height of image to capture from USB camera", "");
core::Property GetUSBCamera::Format(  // NOLINT
    "Format",
    "Frame format (currently only PNG and RAW are supported; RAW is a binary pixel buffer of RGB values)",
    "PNG");
core::Property GetUSBCamera::VendorID(  // NOLINT
    "USB Vendor ID", "USB Vendor ID of camera device, in hexadecimal format", "0x0");
core::Property GetUSBCamera::ProductID(  // NOLINT
    "USB Product ID", "USB Product ID of camera device, in hexadecimal format", "0x0");
core::Property GetUSBCamera::SerialNo(  // NOLINT
    "USB Serial No.", "USB Serial No. of camera device", "");
core::Relationship GetUSBCamera::Success(  // NOLINT
    "success", "Sucessfully captured images sent here");
core::Relationship GetUSBCamera::Failure(  // NOLINT
    "failure", "Failures sent here");

void GetUSBCamera::initialize() {
  std::set<core::Property> properties;
  properties.insert(FPS);
  properties.insert(Width);
  properties.insert(Height);
  properties.insert(Format);
  properties.insert(VendorID);
  properties.insert(ProductID);
  properties.insert(SerialNo);
  setSupportedProperties(properties);

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void GetUSBCamera::onFrame(uvc_frame_t *frame, void *ptr) {
  auto cb_data = reinterpret_cast<GetUSBCamera::CallbackData *>(ptr);
  std::unique_lock<std::recursive_mutex> lock(*(cb_data->dev_access_mtx), std::try_to_lock);

  if (!lock.owns_lock()) {
    return;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

  if (now - cb_data->last_frame_time < std::chrono::milliseconds(static_cast<int>(1000.0 / cb_data->target_fps))) {
    return;
  }

  cb_data->last_frame_time = now;

  try {
    uvc_error_t ret;
    cb_data->logger->log_debug("Got frame");

    ret = uvc_any2rgb(frame, cb_data->frame_buffer);

    if (ret) {
      cb_data->logger->log_error("Failed to convert frame to RGB: %s", uvc_strerror(ret));
      return;
    }

    auto session = cb_data->session_factory->createSession();
    auto flow_file = session->create();

    std::string flow_file_name;
    flow_file->getAttribute("filename", flow_file_name);
    cb_data->logger->log_info("Created flow file: %s", flow_file_name);

    // Initialize callback according to output format
    std::shared_ptr<OutputStreamCallback> write_cb;

    if (cb_data->format == "PNG") {
      write_cb = std::make_shared<GetUSBCamera::PNGWriteCallback>(cb_data->png_write_mtx,
                                                                  cb_data->frame_buffer,
                                                                  cb_data->device_width,
                                                                  cb_data->device_height);
    } else if (cb_data->format == "RAW") {
      write_cb = std::make_shared<GetUSBCamera::RawWriteCallback>(cb_data->frame_buffer);
    } else {
      cb_data->logger->log_warn("Invalid format specified (%s); defaulting to PNG", cb_data->format);
      write_cb = std::make_shared<GetUSBCamera::PNGWriteCallback>(cb_data->png_write_mtx,
                                                                  cb_data->frame_buffer,
                                                                  cb_data->device_width,
                                                                  cb_data->device_height);
    }

    session->write(flow_file, write_cb.get());
    session->transfer(flow_file, GetUSBCamera::Success);
    session->commit();
  } catch (std::exception &exception) {
    cb_data->logger->log_debug("GetUSBCamera Caught Exception %s", exception.what());
  } catch (...) {
    cb_data->logger->log_debug("GetUSBCamera Caught Unknown Exception");
  }
}

void GetUSBCamera::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *session_factory) {
  std::lock_guard<std::recursive_mutex> lock(*dev_access_mtx_);

  double default_fps = 1;
  double target_fps = default_fps;
  std::string conf_fps_str;
  context->getProperty("FPS", conf_fps_str);

  if (conf_fps_str.empty()) {
    logger_->log_info("FPS property was not set; using default %f", default_fps);
  } else {
    try {
      target_fps = std::stod(conf_fps_str);
    } catch (std::invalid_argument &e) {
      logger_->log_error("Could not parse configured FPS value (will use default %f): %s", default_fps, conf_fps_str);
    }
  }

  uint16_t target_width = 0;
  uint16_t target_height = 0;
  std::string conf_width_str;
  context->getProperty("Width", conf_width_str);

  if (!conf_width_str.empty()) {
    auto target_width_ul = std::stoul(conf_width_str);
    if (target_width_ul > UINT16_MAX) {
      logger_->log_error("Configured target width %s is out of range", conf_width_str);
    } else {
      target_width = static_cast<uint16_t>(target_width_ul);
    }
    logger_->log_info("Using configured target width: %i", target_width);
  }

  std::string conf_height_str;
  context->getProperty("Height", conf_height_str);

  if (!conf_height_str.empty()) {
    auto target_height_ul = std::stoul(conf_height_str);
    if (target_height_ul > UINT16_MAX) {
      logger_->log_error("Configured target height %s is out of range", conf_height_str);
    } else {
      target_height = static_cast<uint16_t>(target_height_ul);
    }
    logger_->log_info("Using configured target height: %i", target_height);
  }

  std::string conf_format_str;
  context->getProperty("Format", conf_format_str);

  int usb_vendor_id;
  std::string conf_vendor_id;
  context->getProperty("USB Vendor ID", conf_vendor_id);
  std::stringstream(conf_vendor_id) >> std::hex >> usb_vendor_id;
  logger_->log_info("Using USB Vendor ID: %x", usb_vendor_id);

  int usb_product_id;
  std::string conf_product_id;
  context->getProperty("USB Product ID", conf_product_id);
  std::stringstream(conf_product_id) >> std::hex >> usb_product_id;
  logger_->log_info("Using USB Product ID: %x", usb_product_id);

  const char *usb_serial_no = nullptr;
  std::string conf_serial;
  context->getProperty("USB Serial No.", conf_serial);

  if (!conf_serial.empty()) {
    usb_serial_no = conf_serial.c_str();
    logger_->log_info("Using USB Serial No.: %s", conf_serial);
  }

  cleanupUvc();
  logger_->log_info("Beginning to capture frames from USB camera");

  uvc_stream_ctrl_t ctrl{};
  uvc_error_t res;
  res = uvc_init(&ctx_, nullptr);

  if (res < 0) {
    logger_->log_error("Failed to initialize UVC service context");
    ctx_ = nullptr;
    return;
  }

  logger_->log_debug("UVC initialized");

  // Locate device
  res = uvc_find_device(ctx_, &dev_, usb_vendor_id, usb_product_id, usb_serial_no);

  if (res < 0) {
    logger_->log_error("Unable to find device: %s", uvc_strerror(res));
    dev_ = nullptr;
  } else {
    logger_->log_info("Device found");

    // Open the device
    res = uvc_open(dev_, &devh_);

    if (res < 0) {
      logger_->log_error("Unable to open device: %s", uvc_strerror(res));
      devh_ = nullptr;
    } else {
      logger_->log_info("Device opened");

      // Iterate resolutions & framerates >= context fps, or nearest
      uint16_t width = 0;
      uint16_t height = 0;
      uint32_t max_size = 0;
      uint32_t fps = 0;

      double min_diff = -1;
      double current_diff = -1;
      double current_width_diff = -1;
      double current_height_diff = -1;

      for (auto fmt_desc = uvc_get_format_descs(devh_); fmt_desc; fmt_desc = fmt_desc->next) {
        uvc_frame_desc_t *frame_desc;
        switch (fmt_desc->bDescriptorSubtype) {
          case UVC_VS_FORMAT_UNCOMPRESSED:
          case UVC_VS_FORMAT_FRAME_BASED:
            for (frame_desc = fmt_desc->frame_descs; frame_desc; frame_desc = frame_desc->next) {
              uint32_t frame_fps = 10000000 / frame_desc->dwDefaultFrameInterval;
              logger_->log_info("Discovered supported format %ix%i @ %i",
                                frame_desc->wWidth,
                                frame_desc->wHeight,
                                frame_fps);
              if (target_height > 0 && target_width > 0) {
                if (frame_fps >= target_fps) {
                  current_width_diff = abs(frame_desc->wWidth - target_width) / static_cast<double>(target_width);
                  logger_->log_debug("Current frame format width difference is %f", current_width_diff);
                  current_height_diff = abs(frame_desc->wHeight - target_height) / static_cast<double>(target_height);
                  logger_->log_debug("Current frame format height difference is %f", current_height_diff);
                  current_diff = (current_width_diff + current_height_diff) / 2;
                  logger_->log_debug("Current frame format difference is %f", current_diff);

                  if (min_diff < 0 || current_diff < min_diff) {
                    logger_->log_info("Format %ix%i @ %i is now closest to target",
                                      frame_desc->wWidth,
                                      frame_desc->wHeight,
                                      frame_fps);
                    width = frame_desc->wWidth;
                    height = frame_desc->wHeight;
                    max_size = frame_desc->dwMaxVideoFrameBufferSize;
                    fps = frame_fps;
                    min_diff = current_diff;
                  }
                }

              } else {
                if (frame_desc->dwMaxVideoFrameBufferSize > max_size && frame_fps >= target_fps) {
                  width = frame_desc->wWidth;
                  height = frame_desc->wHeight;
                  max_size = frame_desc->dwMaxVideoFrameBufferSize;
                  fps = frame_fps;
                }
              }
            }
            break;

          case UVC_VS_FORMAT_MJPEG:
            logger_->log_info("Skipping MJPEG frame formats");
            break;

          default:logger_->log_warn("Found unknown format");
        }
      }

      if (fps == 0) {
        logger_->log_error("Could not find suitable frame format from device. "
                           "Try changing configuration (lower FPS) or device.");
        return;
      }

      logger_->log_info("Negotiating stream profile (looking for %dx%d @ %d)", width, height, fps);

      res = uvc_get_stream_ctrl_format_size(devh_, &ctrl, UVC_FRAME_FORMAT_UNCOMPRESSED, width, height, fps);

      if (res < 0) {
        logger_->log_error("Failed to find a matching stream profile: %s", uvc_strerror(res));
      } else {
        cb_data_.session_factory = session_factory;

        if (frame_buffer_ != nullptr) {
          uvc_free_frame(frame_buffer_);
        }

        frame_buffer_ = uvc_allocate_frame(width * height * 3);

        if (!frame_buffer_) {
          printf("unable to allocate bgr frame!");
          logger_->log_error("Unable to allocate RGB frame");
          return;
        }

        cb_data_.frame_buffer = frame_buffer_;
        cb_data_.context = context;
        cb_data_.png_write_mtx = png_write_mtx_;
        cb_data_.dev_access_mtx = dev_access_mtx_;
        cb_data_.logger = logger_;
        cb_data_.format = conf_format_str;
        cb_data_.device_width = width;
        cb_data_.device_height = height;
        cb_data_.device_fps = fps;
        cb_data_.target_fps = target_fps;
        cb_data_.last_frame_time = std::chrono::milliseconds(0);

        res = uvc_start_streaming(devh_, &ctrl, onFrame, &cb_data_, 0);

        if (res < 0) {
          logger_->log_error("Unable to start streaming: %s", uvc_strerror(res));
        } else {
          logger_->log_info("Streaming...");

          // Enable auto-exposure
          uvc_set_ae_mode(devh_, 1);
        }
      }
    }
  }
}

void GetUSBCamera::cleanupUvc() {
  std::lock_guard<std::recursive_mutex> lock(*dev_access_mtx_);

  if (frame_buffer_ != nullptr) {
    logger_->log_debug("Deallocating frame buffer");
    uvc_free_frame(frame_buffer_);
  }

  if (devh_ != nullptr) {
    logger_->log_debug("Stopping UVC streaming");
    uvc_stop_streaming(devh_);
    logger_->log_debug("Closing UVC device handle");
    uvc_close(devh_);
  }

  if (dev_ != nullptr) {
    logger_->log_debug("Closing UVC device descriptor");
    uvc_unref_device(dev_);
  }

  if (ctx_ != nullptr) {
    logger_->log_debug("Closing UVC context");
    uvc_exit(ctx_);
  }

  if (camera_thread_ != nullptr) {
    camera_thread_->join();
    logger_->log_debug("UVC thread ended");
  }
}

void GetUSBCamera::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession *session) {
  auto flowFile = session->get();

  if (flowFile) {
    logger_->log_error("Received flowfile, but this processor does not support input flow files; routing to failure");
    session->transfer(flowFile, Failure);
  }
}

GetUSBCamera::PNGWriteCallback::PNGWriteCallback(std::shared_ptr<std::mutex> write_mtx,
                                                 uvc_frame_t *frame,
                                                 uint32_t width,
                                                 uint32_t height)
    : png_write_mtx_(std::move(write_mtx)),
      frame_(frame),
      width_(width),
      height_(height),
      logger_(logging::LoggerFactory<PNGWriteCallback>::getLogger()) {
}

int64_t GetUSBCamera::PNGWriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  std::lock_guard<std::mutex> lock(*png_write_mtx_);
  logger_->log_info("Writing %d bytes of raw capture data to PNG output", frame_->data_bytes);
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

  if (!png) {
    logger_->log_error("Failed to create PNG write struct");
    return 0;
  }

  png_infop info = png_create_info_struct(png);

  if (!info) {
    logger_->log_error("Failed to create PNG info struct");
    return 0;
  }

  if (setjmp(png_jmpbuf(png))) {
    logger_->log_error("Failed to set PNG jmpbuf");
    return 0;
  }

  try {
    png_set_write_fn(png, this, [](png_structp out_png,
                                   png_bytep out_data,
                                   png_size_t num_bytes) {
                       auto this_callback = reinterpret_cast<PNGWriteCallback *>(png_get_io_ptr(out_png));
                       std::copy(out_data, out_data + num_bytes, std::back_inserter(this_callback->png_output_buf_));
                     },
                     [](png_structp /*flush_png*/) {});

    png_set_IHDR(png, info, width_, height_, 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_bytep> row_pointers;
    row_pointers.resize(height_);

    for (uint32_t y = 0; y < height_; y++) {
      row_pointers[y] = reinterpret_cast<png_byte *>(frame_->data) + width_ * y * 3;
    }

    png_write_image(png, &row_pointers[0]);
    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);
  } catch (...) {
    if (png && info) {
      png_destroy_write_struct(&png, &info);
    }
    throw;
  }

  const auto write_ret = stream->write(png_output_buf_.data(), png_output_buf_.size());
  return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
}

GetUSBCamera::RawWriteCallback::RawWriteCallback(uvc_frame_t *frame)
    : frame_(frame),
      logger_(logging::LoggerFactory<RawWriteCallback>::getLogger()) {
}

int64_t GetUSBCamera::RawWriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  logger_->log_info("Writing %d bytes of raw capture data", frame_->data_bytes);
  const auto write_ret = stream->write(reinterpret_cast<uint8_t*>(frame_->data), frame_->data_bytes);
  return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
}

REGISTER_RESOURCE(GetUSBCamera, "Gets images from USB Video Class (UVC)-compatible devices. Outputs one flow file per frame at the rate specified by the FPS property in the format specified by the Format property.");  // NOLINT line length

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
