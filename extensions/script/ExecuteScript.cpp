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

#ifdef PYTHON_SUPPORT
#include <PythonScriptEngine.h>
#endif  // PYTHON_SUPPORT

#ifdef LUA_SUPPORT
#include <LuaScriptEngine.h>
#endif  // LUA_SUPPORT

#include "ExecuteScript.h"

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
  if (!context->getProperty(ScriptEngine.getName(), script_engine_)) {
    logger_->log_error("Script Engine attribute is missing or invalid");
  }

  context->getProperty(ScriptFile.getName(), script_file_);
  context->getProperty(ScriptBody.getName(), script_body_);
  context->getProperty(ModuleDirectory.getName(), module_directory_);

  if (script_file_.empty() && script_engine_.empty()) {
    logger_->log_error("Either Script Body or Script File must be defined");
    return;
  }
}

void ExecuteScript::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
  try {
    std::shared_ptr<script::ScriptEngine> engine;

    // Use an existing engine, if one is available
    if (script_engine_q_.try_dequeue(engine)) {
      logger_->log_debug("Using available %s script engine instance", script_engine_);
    } else {
      logger_->log_info("Creating new %s script instance", script_engine_);
      logger_->log_info("Approximately %d %s script instances created for this processor",
                        script_engine_q_.size_approx(),
                        script_engine_);

      if (script_engine_ == "python") {
#ifdef PYTHON_SUPPORT
        engine = createEngine<python::PythonScriptEngine>();
#else
        throw std::runtime_error("Python support is disabled in this build.");
#endif  // PYTHON_SUPPORT
      } else if (script_engine_ == "lua") {
#ifdef LUA_SUPPORT
        engine = createEngine<lua::LuaScriptEngine>();
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
#else
      throw std::runtime_error("Lua support is disabled in this build.");
#endif  // LUA_SUPPORT
    }

    // Make engine available for use again
    if (script_engine_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing %s script engine", script_engine_);
      script_engine_q_.enqueue(engine);
    } else {
      logger_->log_info("Destroying script engine because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
    this->yield();
  } catch (...) {
    logger_->log_error("Caught Exception");
    this->yield();
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
