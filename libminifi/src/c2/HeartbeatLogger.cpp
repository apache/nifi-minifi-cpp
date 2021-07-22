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

#include "c2/HeartbeatLogger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

HeartbeatLogger::HeartbeatLogger(const std::string& name, const utils::Identifier& id)
  : HeartbeatReporter(name, id),
    logger_(logging::LoggerFactory<HeartbeatLogger>::getLogger()) {
  logger_->set_max_log_size(-1);  // log however huge the heartbeat is
}

int16_t HeartbeatLogger::heartbeat(const C2Payload &heartbeat) {
  std::string serialized = serializeJsonRootPayload(heartbeat);
  logger_->log_trace("%s", serialized);
  return 0;
}

void HeartbeatLogger::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink, const std::shared_ptr<Configure> &configure) {
  HeartbeatReporter::initialize(controller, updateSink, configure);
  RESTProtocol::initialize(controller, configure);
}

REGISTER_RESOURCE(HeartbeatLogger, "Logs heartbeats at TRACE level.");

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
