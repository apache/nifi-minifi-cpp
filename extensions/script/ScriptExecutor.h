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
#include <utility>
#include <string>

#include "Core.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

namespace org::apache::nifi::minifi::extensions::script {

class ScriptExecutor : public minifi::core::CoreComponent {
 public:
  ScriptExecutor(std::string name, const utils::Identifier& uuid) : core::CoreComponent(std::move(name), uuid) {}

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) = 0;
  virtual void initialize(std::filesystem::path script_file,
      std::string script_body,
      std::optional<std::filesystem::path> module_directory,
      size_t max_concurrent_engines,
      const core::Relationship& success,
      const core::Relationship& failure,
      std::shared_ptr<core::logging::Logger> logger) = 0;

 protected:
  void setScriptFile(std::filesystem::path script_file) {
    script_file_ = std::move(script_file);
  }

  void setScriptBody(std::string script_body) {
    script_body_ = std::move(script_body);
  }

  void setModuleDirectory(std::optional<std::filesystem::path> module_directory) {
    module_directory_ = std::move(module_directory);
  }

  std::filesystem::path script_file_;
  std::string script_body_;
  std::optional<std::filesystem::path> module_directory_;
};
}  // namespace org::apache::nifi::minifi::extensions::script
