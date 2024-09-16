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
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::python {

PythonScriptExecutor::PythonScriptExecutor(const std::string_view name, const utils::Identifier& uuid) : script::ScriptExecutor(name, uuid) {}


void PythonScriptExecutor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(python_script_engine_);
  gsl_Expects(std::holds_alternative<std::filesystem::path>(script_to_run_) || std::holds_alternative<std::string>(script_to_run_));

  if (module_directory_) {
    python_script_engine_->appendModulePaths(utils::string::splitAndTrimRemovingEmpty(*module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }

  if (std::holds_alternative<std::filesystem::path>(script_to_run_))
    python_script_engine_->evalFile(std::get<std::filesystem::path>(script_to_run_));
  else
    python_script_engine_->eval(std::get<std::string>(script_to_run_));

  python_script_engine_->onTrigger(context, session);
}

void PythonScriptExecutor::initialize(std::filesystem::path script_file,
    std::string script_body,
    std::optional<std::string> module_directory,
    size_t /*max_concurrent_engines*/,
    const core::Relationship& success,
    const core::Relationship& failure,
    const core::Relationship& original,
    const std::shared_ptr<core::logging::Logger>& logger) {
  if (script_file.empty() == script_body.empty())
    throw std::runtime_error("Exactly one of these must be non-empty: ScriptBody, ScriptFile");

  if (!script_file.empty()) {
    script_to_run_.emplace<std::filesystem::path>(std::move(script_file));
  }
  if (!script_body.empty()) {
    script_to_run_.emplace<std::string>(std::move(script_body));
  }
  module_directory_ = std::move(module_directory);

  python_script_engine_ = std::make_unique<python::PythonScriptEngine>();
  python_script_engine_->initialize(success, failure, original, logger);
}

REGISTER_RESOURCE(PythonScriptExecutor, InternalResource);

}  // namespace org::apache::nifi::minifi::extensions::python
