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

#include <memory>
#include <string>

#include "minifi-cpp/core/logging/Logger.h"
#include "HeartbeatReporter.h"
#include "c2/protocols/RESTProtocol.h"

namespace org::apache::nifi::minifi::c2 {

class HeartbeatLogger : public RESTProtocol, public HeartbeatReporter {
 public:
  MINIFIAPI static constexpr const char* Description = "Logs heartbeats at TRACE level.";

  explicit HeartbeatLogger(std::string_view name, const utils::Identifier& id = {});
  int16_t heartbeat(const C2Payload &heartbeat) override;
  void initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink, const std::shared_ptr<Configure> &configure) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HeartbeatLogger>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
