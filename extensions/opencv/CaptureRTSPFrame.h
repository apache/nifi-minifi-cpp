/**
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

#include <atomic>
#include <iomanip>
#include <ctime>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <opencv2/opencv.hpp>

#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "io/StreamPipe.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class CaptureRTSPFrame : public core::Processor {
 public:
  explicit CaptureRTSPFrame(std::string name, const utils::Identifier &uuid = {})
      : Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Captures a frame from the RTSP stream at specified intervals.";

  EXTENSIONAPI static const core::Property RTSPUsername;
  EXTENSIONAPI static const core::Property RTSPPassword;
  EXTENSIONAPI static const core::Property RTSPHostname;
  EXTENSIONAPI static const core::Property RTSPURI;
  EXTENSIONAPI static const core::Property RTSPPort;
  EXTENSIONAPI static const core::Property ImageEncoding;
  static auto properties() {
    return std::array{
      RTSPUsername,
      RTSPPassword,
      RTSPHostname,
      RTSPURI,
      RTSPPort,
      ImageEncoding
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

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

  void notifyStop() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CaptureRTSPFrame>::getLogger(uuid_);
  std::mutex mutex_;
  std::string rtsp_username_;
  std::string rtsp_password_;
  std::string rtsp_host_;
  std::string rtsp_port_;
  std::string rtsp_uri_;
  std::string rtsp_url_;
  cv::VideoCapture video_capture_;
  std::string image_encoding_;
  std::string video_backend_driver_;

//  std::function<int()> f_ex;
//
//  std::atomic<bool> running_;
//
//  std::unique_ptr<DataHandler> handler_;
//
//  std::vector<std::string> endpoints;
//
//  std::map<std::string, std::future<int>*> live_clients_;
//
//  utils::ThreadPool<int> client_thread_pool_;
//
//  moodycamel::ConcurrentQueue<std::unique_ptr<io::Socket>> socket_ring_buffer_;
//
//  bool stay_connected_;
//
//  uint16_t concurrent_handlers_;
//
//  int8_t endOfMessageByte;
//
//  uint64_t reconnect_interval_;
//
//  uint64_t receive_buffer_size_;
//
//  uint16_t connection_attempt_limit_;
//
//  std::shared_ptr<GetTCPMetrics> metrics_;
//
//  // Mutex for ensuring clients are running
//
//  std::mutex mutex_;
//
//  std::shared_ptr<minifi::controllers::SSLContextService> ssl_service_;
};

}   // namespace org::apache::nifi::minifi::processors
