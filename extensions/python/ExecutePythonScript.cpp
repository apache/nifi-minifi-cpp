/**
 * @file ExecutePythonScript.cpp

 * ExecutePythonScript class implementation
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

#include "ExecutePythonScript.h"


#include <memory>
#include <vector>

#include "PythonScriptEngine.h"
#include "types/PyLogger.h"
#include "types/PyRelationship.h"

#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "range/v3/range/conversion.hpp"

namespace org::apache::nifi::minifi::extensions::python {

const core::Property ExecutePythonScript::ScriptFile("Script File",
    R"(Path to script file to execute. Only one of Script File or Script Body may be used)", "");
const core::Property ExecutePythonScript::ScriptBody("Script Body",
    R"(Body of script to execute. Only one of Script File or Script Body may be used)", "");
const core::Property ExecutePythonScript::ModuleDirectory("Module Directory",
    R"(Comma-separated list of paths to files and/or directories which contain modules required by the script)", "");

const core::Relationship ExecutePythonScript::Success("success", "Script successes");
const core::Relationship ExecutePythonScript::Failure("failure", "Script failures");

void ExecutePythonScript::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExecutePythonScript::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  python_script_engine_ = std::make_unique<python::PythonScriptEngine>();
  python_script_engine_->bind("log", std::weak_ptr<core::logging::Logger>(logger_));
  python_script_engine_->bind("REL_SUCCESS", Success);
  python_script_engine_->bind("REL_FAILURE", Failure);

  context->getProperty(ScriptFile.getName(), script_file_);
  context->getProperty(ScriptBody.getName(), script_body_);
  module_directory_ = context->getProperty(ModuleDirectory);

  if (script_file_.empty() && script_body_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either Script Body or Script File must be defined");
  }

  if (!script_file_.empty() && !script_body_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Only one of Script File or Script Body may be defined!");
  }

  if (!script_file_.empty() && !std::filesystem::is_regular_file(std::filesystem::status(script_file_))) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Script File set is not a regular file or does not exist: " + script_file_);
  }
}

void ExecutePythonScript::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
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

REGISTER_RESOURCE(ExecutePythonScript, Processor);

}  // namespace org::apache::nifi::minifi::extensions::python
