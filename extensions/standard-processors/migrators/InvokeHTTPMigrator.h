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

#include "core/flow/FlowMigrator.h"
#include "core/flow/FlowSchema.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::standard::migration {

class InvokeHTTPMigrator final : public core::flow::FlowMigrator {
 public:
  explicit InvokeHTTPMigrator(const std::string_view name, const utils::Identifier& uuid = {}) : FlowMigrator(name, uuid) {}

  void migrate(core::flow::Node& root_node, const core::flow::FlowSchema& schema) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<InvokeHTTPMigrator>::getLogger();
};


}  // namespace org::apache::nifi::minifi::standard::migration
