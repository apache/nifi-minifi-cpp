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

#include <string>
#include <variant>

#include "core/Core.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::extensions::script {

class ScriptExecutor : public minifi::core::CoreComponentImpl {
 public:
  ScriptExecutor(const std::string_view name, const utils::Identifier& uuid) : core::CoreComponentImpl(name, uuid) {}

  virtual void onTrigger(core::ProcessContext& context, core::ProcessSession& session) = 0;
  virtual void initialize(std::filesystem::path script_file,
      std::string script_body,
      std::optional<std::string> module_directory,
      size_t max_concurrent_engines,
      const core::Relationship& success,
      const core::Relationship& failure,
      const core::Relationship& original,
      const std::shared_ptr<core::logging::Logger>& logger) = 0;

 protected:
  std::variant<std::monostate, std::filesystem::path, std::string> script_to_run_;
  std::optional<std::string> module_directory_;
};
}  // namespace org::apache::nifi::minifi::extensions::script
