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
#include <set>
#include <utility>

#ifdef LUA_SUPPORT
#include <LuaScriptEngine.h>
#endif  // LUA_SUPPORT

#include "ExecuteScript.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ExecuteScript::ScriptEngine("Script Engine",  // NOLINT
    R"(The engine to execute scripts (python, lua))", "python");
core::Property ExecuteScript::ScriptFile("Script File",  // NOLINT
    R"(Path to script file to execute. Only one of Script File or Script Body may be used)", "");
core::Property ExecuteScript::ScriptBody("Script Body",  // NOLINT
    R"(Body of script to execute. Only one of Script File or Script Body may be used)", "");
core::Property ExecuteScript::ModuleDirectory("Module Directory",  // NOLINT
    R"(Comma-separated list of paths to files and/or directories which contain modules required by the script)", "");

core::Relationship ExecuteScript::Success("success", "Script successes");  // NOLINT
core::Relationship ExecuteScript::Failure("failure", "Script failures");  // NOLINT

ScriptEngineFactory::ScriptEngineFactory(core::Relationship& success, core::Relationship& failure, std::shared_ptr<core::logging::Logger> logger)
  : success_(success),
    failure_(failure),
    logger_(logger) {
}

ScriptEngineQueue::ScriptEngineQueue(uint8_t max_engine_count, ScriptEngineFactory& engine_factory, std::shared_ptr<core::logging::Logger> logger)
    : max_engine_count_(max_engine_count),
      engine_factory_(engine_factory),
      logger_(logger) {
}

void ScriptEngineQueue::returnScriptEngine(std::shared_ptr<script::ScriptEngine>&& engine) {
  const std::lock_guard<std::mutex> lock(queue_mutex_);
  if (engine_queue_.size_approx() < max_engine_count_) {
    logger_->log_debug("Releasing [%p] script engine", engine.get());
    engine_queue_.enqueue(engine);
  } else {
    logger_->log_info("Destroying script engine because it is no longer needed");
  }
}

void ExecuteScript::initialize() {
  std::set<core::Property> properties;
  properties.insert(ScriptEngine);
  properties.insert(ScriptFile);
  properties.insert(ScriptBody);
  properties.insert(ModuleDirectory);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));

#ifdef PYTHON_SUPPORT
  python::PythonScriptEngine::initialize();
#endif  // PYTHON_SUPPORT
}

void ExecuteScript::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
#ifdef LUA_SUPPORT
  script_engine_q_ = std::make_unique<ScriptEngineQueue>(getMaxConcurrentTasks(), engine_factory_, logger_);
#endif  // LUA_SUPPORT
#ifdef PYTHON_SUPPORT
  python_script_engine_ = engine_factory_.createEngine<python::PythonScriptEngine>();
#endif  // PYTHON_SUPPORT
  if (!context->getProperty(ScriptEngine.getName(), script_engine_)) {
    logger_->log_error("Script Engine attribute is missing or invalid");
  }

  context->getProperty(ScriptFile.getName(), script_file_);
  context->getProperty(ScriptBody.getName(), script_body_);
  context->getProperty(ModuleDirectory.getName(), module_directory_);

  if (script_file_.empty() && script_body_.empty()) {
    logger_->log_error("Either Script Body or Script File must be defined");
    return;
  }
}

void ExecuteScript::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<script::ScriptEngine> engine;

  if (script_engine_ == "python") {
#ifdef PYTHON_SUPPORT
    engine = python_script_engine_;
#else
    throw std::runtime_error("Python support is disabled in this build.");
#endif  // PYTHON_SUPPORT
  } else if (script_engine_ == "lua") {
#ifdef LUA_SUPPORT
    engine = script_engine_q_->getScriptEngine<lua::LuaScriptEngine>();
#else
    throw std::runtime_error("Lua support is disabled in this build.");
#endif  // LUA_SUPPORT
  }

  if (engine == nullptr) {
    throw std::runtime_error("No script engine available");
  }

  if (!script_body_.empty()) {
    engine->eval(script_body_);
  } else if (!script_file_.empty()) {
    engine->evalFile(script_file_);
  } else {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  if (script_engine_ == "python") {
#ifdef PYTHON_SUPPORT
    triggerEngineProcessor<python::PythonScriptEngine>(engine, context, session);
#else
    throw std::runtime_error("Python support is disabled in this build.");
#endif  // PYTHON_SUPPORT
  } else if (script_engine_ == "lua") {
#ifdef LUA_SUPPORT
    triggerEngineProcessor<lua::LuaScriptEngine>(engine, context, session);
    script_engine_q_->returnScriptEngine(std::move(engine));
#else
    throw std::runtime_error("Lua support is disabled in this build.");
#endif  // LUA_SUPPORT
  }
}

REGISTER_RESOURCE(ExecuteScript, "Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) "
    "as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context"
    " and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
    "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
    "concurrent tasks to 1."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
