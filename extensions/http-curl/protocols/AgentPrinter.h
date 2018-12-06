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
#ifndef LIBMINIFI_INCLUDE_C2_AGENTPRINTER_H_
#define LIBMINIFI_INCLUDE_C2_AGENTPRINTER_H_

#include <string>
#include <mutex>
#include "core/Resource.h"
#include "c2/protocols/RESTProtocol.h"
#include "CivetServer.h"
#include "c2/C2Protocol.h"
#include "controllers/SSLContextService.h"

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
class AgentPrinter : public RESTProtocol, public HeartBeatReporter {
 public:
  AgentPrinter(std::string name, utils::Identifier uuid = utils::Identifier());

  /**
   * Initialize agent printer.
   */
  virtual void initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                          const std::shared_ptr<Configure> &configure) override;

  /**
   * Accepts the heartbeat, only extracting AgentInformation.
   */
  virtual int16_t heartbeat(const C2Payload &heartbeat) override;

  /**
   * Overrides extracting the agent information from the root.
   */
  virtual std::string serializeJsonRootPayload(const C2Payload& payload) override;

  /**
   * Overrides extracting the agent information from the payload.
   */
  virtual rapidjson::Value serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) override;

 protected:

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(AgentPrinter, "Encapsulates printing agent information.");

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_AGENTPRINTER_H_ */
