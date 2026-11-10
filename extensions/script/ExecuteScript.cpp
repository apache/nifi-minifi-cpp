// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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


  std::string script_file = context.getProperty(ScriptFile).value_or("");
  std::string script_body = context.getProperty(ScriptBody).value_or("");

  auto module_directory = context.getProperty(ModuleDirectory) | utils::toOptional();

  if (script_file.empty() && script_body.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either Script Body or Script File must be defined");
  }

  if (!script_file.empty() && !script_body.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Only one of Script File or Script Body may be defined!");
  }

  if (!script_file.empty() && !std::filesystem::is_regular_file(std::filesystem::status(script_file))) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Script File set is not a regular file or does not exist: " + script_file);
  }

  script_executor_->initialize(std::move(script_file), std::move(script_body), std::move(module_directory), context.getMaxConcurrentTasks(), Success, Failure, Original, logger_);
}

void ExecuteScript::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(script_executor_);
  script_executor_->onTrigger(context, session);
}

REGISTER_RESOURCE(ExecuteScript, Processor);

}  // namespace org::apache::nifi::minifi::processors
