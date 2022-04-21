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

#include <memory>
#include <utility>

#ifdef PYTHON_SUPPORT
#include <PythonScriptEngine.h>
#endif  // PYTHON_SUPPORT

#include "ExecuteScript.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ExecuteScript::ScriptEngine(
  core::PropertyBuilder::createProperty("Script Engine")
    ->withDescription(R"(The engine to execute scripts (python, lua))")
    ->isRequired(true)
    ->withAllowableValues(ScriptEngineOption::values())
    ->withDefaultValue(toString(ScriptEngineOption::PYTHON))
    ->build());
core::Property ExecuteScript::ScriptFile("Script File",
    R"(Path to script file to execute. Only one of Script File or Script Body may be used)", "");
core::Property ExecuteScript::ScriptBody("Script Body",
    R"(Body of script to execute. Only one of Script File or Script Body may be used)", "");
core::Property ExecuteScript::ModuleDirectory("Module Directory",
    R"(Comma-separated list of paths to files and/or directories which contain modules required by the script)", "");

core::Relationship ExecuteScript::Success("success", "Script successes");
core::Relationship ExecuteScript::Failure("failure", "Script failures");

ScriptEngineFactory::ScriptEngineFactory(core::Relationship& success, core::Relationship& failure, std::shared_ptr<core::logging::Logger> logger)
  : success_(success),
    failure_(failure),
    logger_(logger) {
}

void ExecuteScript::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());

#ifdef PYTHON_SUPPORT
  python::PythonScriptEngine::initialize();
#endif  // PYTHON_SUPPORT
}

void ExecuteScript::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
#ifdef LUA_SUPPORT
  script_engine_q_ = std::make_unique<ScriptEngineQueue<lua::LuaScriptEngine>>(getMaxConcurrentTasks(), engine_factory_, logger_);
#endif  // LUA_SUPPORT
#ifdef PYTHON_SUPPORT
  python_script_engine_ = engine_factory_.createEngine<python::PythonScriptEngine>();
#endif  // PYTHON_SUPPORT

  script_engine_ = ScriptEngineOption::parse(utils::parsePropertyWithAllowableValuesOrThrow(*context, ScriptEngine.getName(), ScriptEngineOption::values()).c_str());

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

void ExecuteScript::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<script::ScriptEngine> engine;

  if (script_engine_ == ScriptEngineOption::PYTHON) {
#ifdef PYTHON_SUPPORT
    engine = python_script_engine_;
#else
    throw std::runtime_error("Python support is disabled in this build.");
#endif  // PYTHON_SUPPORT
  } else if (script_engine_ == ScriptEngineOption::LUA) {
#ifdef LUA_SUPPORT
    engine = script_engine_q_->getScriptEngine();
#else
    throw std::runtime_error("Lua support is disabled in this build.");
#endif  // LUA_SUPPORT
  }

  if (engine == nullptr) {
    throw std::runtime_error("No script engine available");
  }

  if (module_directory_) {
    engine->setModulePaths(utils::StringUtils::splitAndTrimRemovingEmpty(*module_directory_, ","));
  }

  if (!script_body_.empty()) {
    engine->eval(script_body_);
  } else if (!script_file_.empty()) {
    engine->evalFile(script_file_);
  } else {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  if (script_engine_ == ScriptEngineOption::PYTHON) {
#ifdef PYTHON_SUPPORT
    triggerEngineProcessor<python::PythonScriptEngine>(engine, context, session);
#else
    throw std::runtime_error("Python support is disabled in this build.");
#endif  // PYTHON_SUPPORT
  } else if (script_engine_ == ScriptEngineOption::LUA) {
#ifdef LUA_SUPPORT
    triggerEngineProcessor<lua::LuaScriptEngine>(engine, context, session);
    script_engine_q_->returnScriptEngine(std::static_pointer_cast<lua::LuaScriptEngine>(engine));
#else
    throw std::runtime_error("Lua support is disabled in this build.");
#endif  // LUA_SUPPORT
  }
}

REGISTER_RESOURCE(ExecuteScript, Processor);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
