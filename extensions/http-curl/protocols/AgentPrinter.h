/**
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

#include <string>
#include <memory>
#include "core/Resource.h"
#include "c2/protocols/RESTProtocol.h"
#include "c2/C2Protocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Purpose and Justification: Encapsulates printing agent information.
 *
 * Will be used to print agent information from the C2 response to stdout, selecting the agent's manifest
 *
 */
class AgentPrinter : public HeartbeatJsonSerializer, public HeartbeatReporter {
 public:
  explicit AgentPrinter(const std::string& name, const utils::Identifier& uuid = {});

  /**
   * Initialize agent printer.
   */
  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                          const std::shared_ptr<Configure> &configure) override;

  /**
   * Accepts the heartbeat, only extracting AgentInformation.
   */
  int16_t heartbeat(const C2Payload &heartbeat) override;

  /**
   * Overrides extracting the agent information from the payload.
   */
  rapidjson::Value serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
