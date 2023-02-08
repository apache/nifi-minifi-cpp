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
#include "PythonScriptExecutor.h"

#include <string>
#include <filesystem>
#include <vector>
#include <utility>

#include "PythonScriptEngine.h"
#include "range/v3/range/conversion.hpp"
#include "Resource.h"

namespace org::apache::nifi::minifi::extensions::python {

PythonScriptExecutor::PythonScriptExecutor(std::string name, const utils::Identifier& uuid) : script::ScriptExecutor(std::move(name), uuid) {}


void PythonScriptExecutor::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  if (python_script_engine_ == nullptr) {
    throw std::runtime_error("No script engine available");
  }

  if (module_directory_) {
    python_script_engine_->setModulePaths(utils::StringUtils::splitAndTrimRemovingEmpty(*module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }

  if (!script_body_.empty()) {
    python_script_engine_->eval(script_body_);
  } else if (!script_file_.empty()) {
    python_script_engine_->evalFile(script_file_);
  } else {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  python_script_engine_->onTrigger(context, session);
}

void PythonScriptExecutor::initialize(std::filesystem::path script_file,
    std::string script_body,
    std::optional<std::filesystem::path> module_directory,
    size_t /*max_concurrent_engines*/,
    const core::Relationship& success,
    const core::Relationship& failure,
    std::shared_ptr<core::logging::Logger> logger) {
  script_file_ = std::move(script_file);
  script_body_ = std::move(script_body);
  module_directory_ = std::move(module_directory);

  python_script_engine_ = std::make_unique<python::PythonScriptEngine>();
  python_script_engine_->initialize(success, failure, logger);
}

REGISTER_RESOURCE(PythonScriptExecutor, InternalResource);

}  // namespace org::apache::nifi::minifi::extensions::python
