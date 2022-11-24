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

#include "ForwardingNode.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi {

const core::Relationship ForwardingNode::Success("success", "FlowFiles are routed to success relationship");

void ForwardingNode::initialize() {
  setSupportedRelationships(relationships());
}

void ForwardingNode::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& session) {
  logger_->log_trace("On trigger %s", getUUIDStr());
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }
  session->transfer(flow_file, Success);
}

}  // namespace org::apache::nifi::minifi
