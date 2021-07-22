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

#include <set>
#include <stdexcept>
#include <utility>

#include "ExecutePythonProcessor.h"

#include "utils/StringUtils.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {
namespace processors {

core::Property ExecutePythonProcessor::ScriptFile(core::PropertyBuilder::createProperty("Script File")
    ->withDescription("Path to script file to execute. Only one of Script File or Script Body may be used")
    ->withDefaultValue("")
    ->build());

core::Property ExecutePythonProcessor::ScriptBody(core::PropertyBuilder::createProperty("Script Body")
    ->withDescription("Script to execute. Only one of Script File or Script Body may be used")
    ->withDefaultValue("")
    ->build());

core::Property ExecutePythonProcessor::ModuleDirectory(core::PropertyBuilder::createProperty("Module Directory")
  ->withDescription("Comma-separated list of paths to files and/or directories which contain modules required by the script")
  ->withDefaultValue("")
  ->build());

core::Relationship ExecutePythonProcessor::Success("success", "Script successes");
core::Relationship ExecutePythonProcessor::Failure("failure", "Script failures");

void ExecutePythonProcessor::initialize() {
  setSupportedProperties({
    ScriptFile,
    ScriptBody,
    ModuleDirectory
  });
  setAcceptAllProperties();
  setSupportedRelationships({
    Success,
    Failure
  });
}

void ExecutePythonProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  python_logger_ = logging::LoggerFactory<ExecutePythonProcessor>::getAliasedLogger(getName());

  getProperty(ModuleDirectory.getName(), module_directory_);

  appendPathForImportModules();
  loadScript();

  if (script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }
  std::shared_ptr<python::PythonScriptEngine> engine = getScriptEngine();
  engine->eval(script_to_exec_);
  auto shared_this = shared_from_this();
  engine->describe(shared_this);
  engine->onInitialize(shared_this);
  engine->onSchedule(context);
  handleEngineNoLongerInUse(std::move(engine));
}

void ExecutePythonProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  try {
    // TODO(hunyadi): When using "Script File" property, we currently re-read the script file content every time the processor is triggered. This should change to single-read when we release 1.0.0
    // https://issues.apache.org/jira/browse/MINIFICPP-1223
    reloadScriptIfUsingScriptFileProperty();
    if (script_to_exec_.empty()) {
      throw std::runtime_error("Neither Script Body nor Script File is available to execute");
    }

    std::shared_ptr<python::PythonScriptEngine> engine = getScriptEngine();
    engine->onTrigger(context, session);
    handleEngineNoLongerInUse(std::move(engine));
  }
  catch (const std::exception &exception) {
    logger_->log_error("Caught Exception: %s", exception.what());
    this->yield();
  }
  catch (...) {
    logger_->log_error("Caught Exception");
    this->yield();
  }
}

// TODO(hunyadi): This is potentially not what we want. See https://issues.apache.org/jira/browse/MINIFICPP-1222
std::shared_ptr<python::PythonScriptEngine> ExecutePythonProcessor::getScriptEngine() {
  std::shared_ptr<python::PythonScriptEngine> engine;
  // Use an existing engine, if one is available
  if (script_engine_q_.try_dequeue(engine)) {
    logger_->log_debug("Using available [%p] script engine instance", engine.get());
    return engine;
  }
  engine = createEngine<python::PythonScriptEngine>();
  logger_->log_info("Created new [%p] script engine instance. Number of instances: approx. %d / %d.", engine.get(), script_engine_q_.size_approx(), getMaxConcurrentTasks());
  if (engine == nullptr) {
    throw std::runtime_error("No script engine available");
  }
  return engine;
}

void ExecutePythonProcessor::handleEngineNoLongerInUse(std::shared_ptr<python::PythonScriptEngine>&& engine) {
  // Make engine available for use again
  if (script_engine_q_.size_approx() < getMaxConcurrentTasks()) {
    logger_->log_debug("Releasing [%p] script engine", engine.get());
    script_engine_q_.enqueue(engine);
  } else {
    logger_->log_info("Destroying script engine because it is no longer needed");
  }
}

void ExecutePythonProcessor::appendPathForImportModules() {
  // TODO(hunyadi): I have spent some time trying to figure out pybind11, but
  // could not get this working yet. It is up to be implemented later
  // https://issues.apache.org/jira/browse/MINIFICPP-1224
  if (module_directory_.size()) {
    logger_->log_error("Not supported property: Module Directory.");
  }
}

void ExecutePythonProcessor::loadScriptFromFile(const std::string& file_path) {
  std::ifstream file_handle(file_path);
  if (!file_handle.is_open()) {
    script_to_exec_ = "";
    throw std::runtime_error("Failed to read Script File: " + file_path);
  }
  script_to_exec_ = std::string{ (std::istreambuf_iterator<char>(file_handle)), (std::istreambuf_iterator<char>()) };
}

void ExecutePythonProcessor::loadScript() {
  std::string script_file;
  std::string script_body;
  getProperty(ScriptFile.getName(), script_file);
  getProperty(ScriptBody.getName(), script_body);
  if (script_file.empty() && script_body.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }
  if (script_file.size()) {
    if (script_body.size()) {
      throw std::runtime_error("Only one of Script File or Script Body may be used");
    }
    loadScriptFromFile(script_file);
    return;
  }
  script_to_exec_ = script_body;
  return;
}

void ExecutePythonProcessor::reloadScriptIfUsingScriptFileProperty() {
  std::string script_file;
  std::string script_body;
  getProperty(ScriptFile.getName(), script_file);
  getProperty(ScriptBody.getName(), script_body);
  if (script_file.size() && script_body.empty()) {
    loadScriptFromFile(script_file);
  }
}

REGISTER_RESOURCE(
    ExecutePythonProcessor, "Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) "
    "as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context"
    " and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
    "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
    "concurrent tasks to 1.");  // NOLINT

} /* namespace processors */
} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
