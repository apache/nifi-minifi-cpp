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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "HeartbeatReporter.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose: Creates a reporter that can handle basic c2 operations for a localized environment
 * through a simple TCP socket.
 */
class ControllerSocketProtocol : public HeartbeatReporter {
 public:
  ControllerSocketProtocol(std::string name, const utils::Identifier& uuid = {}) // NOLINT
      : HeartbeatReporter(std::move(name), uuid) {
  }

  MINIFIAPI static constexpr const char* Description = "Creates a reporter that can handle basic c2 operations for a localized environment through a simple TCP socket.";

  /**
   * Initialize the socket protocol.
   * @param controller controller service provider.
   * @param updateSink update mechanism that will be used to stop/clear elements
   * @param configuration configuration class.
   */
  void initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink,
                          const std::shared_ptr<Configure> &configuration) override;

  /**
   * Handles the heartbeat
   * @param payload incoming payload. From this function we only care about queue metrics.
   */
  int16_t heartbeat(const C2Payload &payload) override;

 protected:
  /**
   * Parses content from the content response.
   */
  void parse_content(const std::vector<C2ContentResponse> &content);

  std::mutex controller_mutex_;

  std::map<std::string, bool> queue_full_;

  std::map<std::string, uint64_t> queue_size_;

  std::map<std::string, uint64_t> queue_max_;

  std::map<std::string, bool> component_map_;

  std::unique_ptr<io::BaseServerSocket> server_socket_;

  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ControllerSocketProtocol>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
