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

#include "AgentPrinter.h"

#include <algorithm>
#include <memory>
#include <string>

#include "rapidjson/prettywriter.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

AgentPrinter::AgentPrinter(const std::string& name, const utils::Identifier& uuid)
    : HeartbeatReporter(name, uuid),
      logger_(logging::LoggerFactory<AgentPrinter>::getLogger()) {
}

void AgentPrinter::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                              const std::shared_ptr<Configure> &configure) {
  HeartbeatReporter::initialize(controller, updateSink, configure);
}
int16_t AgentPrinter::heartbeat(const C2Payload &payload) {
  std::string outputConfig = serializeJsonRootPayload(payload);
  return 0;
}

rapidjson::Value AgentPrinter::serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
  const bool print = payload.getLabel() == "agentManifest";

  rapidjson::Value result = HeartbeatJsonSerializer::serializeJsonPayload(payload, alloc);

  if (print) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    result.Accept(writer);
    std::cout << buffer.GetString() << std::endl;
    std::exit(1);
  }

  return result;
}

REGISTER_RESOURCE(AgentPrinter, "Encapsulates printing agent information.");

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
