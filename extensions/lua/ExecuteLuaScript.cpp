/**
 * @file ExecuteLuaScript.cpp

 * ExecuteLuaScript class implementation
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

#include <memory>
#include <utility>

#include <vector>

#include "ExecuteLuaScript.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "range/v3/range/conversion.hpp"

namespace org::apache::nifi::minifi::extensions::lua {

const core::Property ExecuteLuaScript::ScriptFile("Script File",
    R"(Path to script file to execute. Only one of Script File or Script Body may be used)", "");
const core::Property ExecuteLuaScript::ScriptBody("Script Body",
    R"(Body of script to execute. Only one of Script File or Script Body may be used)", "");
const core::Property ExecuteLuaScript::ModuleDirectory("Module Directory",
    R"(Comma-separated list of paths to files and/or directories which contain modules required by the script)", "");

const core::Relationship ExecuteLuaScript::Success("success", "Script successes");
const core::Relationship ExecuteLuaScript::Failure("failure", "Script failures");

void ExecuteLuaScript::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExecuteLuaScript::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  auto create_engine = [this]() -> std::unique_ptr<LuaScriptEngine> {
    auto engine = std::make_unique<LuaScriptEngine>();

    engine->bind("log", logger_);
    engine->bind("REL_SUCCESS", Success);
    engine->bind("REL_FAILURE", Failure);

    return engine;
  };
  lua_script_engine_queue_ = utils::ResourceQueue<LuaScriptEngine>::create(create_engine, getMaxConcurrentTasks(), std::nullopt, logger_);

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

void ExecuteLuaScript::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::ProcessSession> &session) {
  auto lua_script_engine = lua_script_engine_queue_->getResource();

  if (module_directory_) {
    lua_script_engine->setModulePaths(utils::StringUtils::splitAndTrimRemovingEmpty(*module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }

  if (!script_body_.empty()) {
    lua_script_engine->eval(script_body_);
  } else if (!script_file_.empty()) {
    lua_script_engine->evalFile(script_file_);
  } else {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  lua_script_engine->onTrigger(context, session);
}

REGISTER_RESOURCE(ExecuteLuaScript, Processor);

}  // namespace org::apache::nifi::minifi::extensions::lua
