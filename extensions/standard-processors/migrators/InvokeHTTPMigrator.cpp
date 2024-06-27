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

#include "InvokeHTTPMigrator.h"

#include "core/Resource.h"
#include "flow/FlowSchema.h"

namespace org::apache::nifi::minifi::standard::migration {

void InvokeHTTPMigrator::migrate(core::flow::Node& root_node, const core::flow::FlowSchema& schema) {
  auto invoke_http_processors = getProcessors(root_node, schema, "InvokeHTTP");
  for (auto& invoke_http_processor : invoke_http_processors) {
    auto invoke_http_properties = invoke_http_processor[schema.processor_properties];
    if (invoke_http_properties.remove("Send Body")) {
      logger_->log_warn("Removed deprecated property \"Send Body\" from {}", *invoke_http_processor[schema.identifier].getString());
    }
    if (invoke_http_properties.remove("Disable Peer Verification")) {
      logger_->log_warn("Removed deprecated property \"Disable Peer Verification\" from {}", *invoke_http_processor[schema.identifier].getString());
    }
  }
}

REGISTER_RESOURCE(InvokeHTTPMigrator, FlowMigrator);
}  // namespace org::apache::nifi::minifi::standard::migration

