/**
 * @file ExecuteScript.cpp

 * ExecuteScript class implementation
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

#include "ExecuteScript.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void ExecuteScript::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ExecuteScript::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  const auto executor_class_lookup = [](const std::string_view script_engine_prefix) -> const char* {
    if (script_engine_prefix == "lua") return "LuaScriptExecutor";
    if (script_engine_prefix == "python") return "PythonScriptExecutor";
    return nullptr;
  };
  if (const auto script_engine_prefix = context.getProperty(ScriptEngine); script_engine_prefix && executor_class_lookup(*script_engine_prefix)) {
    const char* const executor_class_name = executor_class_lookup(*script_engine_prefix);
    script_executor_ = core::ClassLoader::getDefaultClassLoader().instantiate<extensions::script::ScriptExecutor>(executor_class_name, executor_class_name);
    if (!script_executor_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Could not instantiate: " + std::string(executor_class_name) + ". Make sure that the " + *script_engine_prefix + " scripting extension is loaded");
    }
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid script engine name");
  }


  std::string script_file;
  std::string script_body;
  context.getProperty(ScriptFile, script_file);
  context.getProperty(ScriptBody, script_body);
  auto module_directory = context.getProperty(ModuleDirectory);

  if (script_file.empty() && script_body.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either Script Body or Script File must be defined");
  }

  if (!script_file.empty() && !script_body.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Only one of Script File or Script Body may be defined!");
  }

  if (!script_file.empty() && !std::filesystem::is_regular_file(std::filesystem::status(script_file))) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Script File set is not a regular file or does not exist: " + script_file);
  }

  script_executor_->initialize(std::move(script_file), std::move(script_body), std::move(module_directory), getMaxConcurrentTasks(), Success, Failure, Original, logger_);
}

void ExecuteScript::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(script_executor_);
  script_executor_->onTrigger(context, session);
}

REGISTER_RESOURCE(ExecuteScript, Processor);

}  // namespace org::apache::nifi::minifi::processors
