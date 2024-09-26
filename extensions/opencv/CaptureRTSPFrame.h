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

#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "io/StreamPipe.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class CaptureRTSPFrame : public core::ProcessorImpl {
 public:
  explicit CaptureRTSPFrame(std::string name, const utils::Identifier &uuid = {})
      : Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Captures a frame from the RTSP stream at specified intervals.";

  EXTENSIONAPI static constexpr auto RTSPUsername = core::PropertyDefinitionBuilder<>::createProperty("RTSP Username")
    .withDescription("The username for connecting to the RTSP stream")
    .build();
  EXTENSIONAPI static constexpr auto RTSPPassword = core::PropertyDefinitionBuilder<>::createProperty("RTSP Password")
    .withDescription("Password used to connect to the RTSP stream")
    .isSensitive(true)
    .build();
  EXTENSIONAPI static constexpr auto RTSPHostname = core::PropertyDefinitionBuilder<>::createProperty("RTSP Hostname")
    .withDescription("Hostname of the RTSP stream we are trying to connect to")
    .build();
  EXTENSIONAPI static constexpr auto RTSPURI = core::PropertyDefinitionBuilder<>::createProperty("RTSP URI")
    .withDescription("URI that should be appended to the RTSP stream hostname")
    .build();
  EXTENSIONAPI static constexpr auto RTSPPort = core::PropertyDefinitionBuilder<>::createProperty("RTSP Port")
    .withDescription("Port that should be connected to to receive RTSP Frames")
    .build();
  EXTENSIONAPI static constexpr auto ImageEncoding = core::PropertyDefinitionBuilder<>::createProperty("Image Encoding")
    .withDescription("The encoding that should be applied the the frame images captured from the RTSP stream")
    .withDefaultValue(".jpg")
    .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      RTSPUsername,
      RTSPPassword,
      RTSPHostname,
      RTSPURI,
      RTSPPort,
      ImageEncoding
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successful capture of RTSP frame"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failures to capture RTSP frame"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

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
