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
#include "utils/file/FileUtils.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::python::processors {

const core::Property ExecutePythonProcessor::ScriptFile(core::PropertyBuilder::createProperty("Script File")
    ->withDescription("Path to script file to execute. Only one of Script File or Script Body may be used")
    ->build());

const core::Property ExecutePythonProcessor::ScriptBody(core::PropertyBuilder::createProperty("Script Body")
    ->withDescription("Script to execute. Only one of Script File or Script Body may be used")
    ->build());

const core::Property ExecutePythonProcessor::ModuleDirectory(core::PropertyBuilder::createProperty("Module Directory")
  ->withDescription("Comma-separated list of paths to files and/or directories which contain modules required by the script")
  ->build());

const core::Property ExecutePythonProcessor::ReloadOnScriptChange(core::PropertyBuilder::createProperty("Reload on Script Change")
  ->withDescription("If true and Script File property is used, then script file will be reloaded if it has changed, otherwise the first loaded version will be used at all times.")
  ->isRequired(true)
  ->withDefaultValue<bool>(true)
  ->build());

const core::Relationship ExecutePythonProcessor::Success("success", "Script succeeds");
const core::Relationship ExecutePythonProcessor::Failure("failure", "Script fails");

void ExecutePythonProcessor::initialize() {
  if (getProperties().empty()) {
    setSupportedProperties(properties());
    setAcceptAllProperties();
    setSupportedRelationships(relationships());
  }

  if (processor_initialized_) {
    logger_->log_debug("Processor has already been initialized, returning...");
    return;
  }

  try {
    loadScript();
  } catch(const std::runtime_error&) {
    logger_->log_warn("Could not load python script while initializing. In case of non-native python processor this is normal and will be done in the schedule phase.");
    return;
  }

  // In case of native python processors we require initialization before onSchedule
  // so that we can provide manifest of processor identity on C2
  python_script_engine_ = createScriptEngine();
  initalizeThroughScriptEngine();
}

void ExecutePythonProcessor::initalizeThroughScriptEngine() {
  appendPathForImportModules();
  python_script_engine_->eval(script_to_exec_);
  python_script_engine_->describe(this);
  python_script_engine_->onInitialize(this);
  processor_initialized_ = true;
}

void ExecutePythonProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  if (!processor_initialized_) {
    loadScript();
    python_script_engine_ = createScriptEngine();
    initalizeThroughScriptEngine();
  } else {
    reloadScriptIfUsingScriptFileProperty();
    if (script_to_exec_.empty()) {
      throw std::runtime_error("Neither Script Body nor Script File is available to execute");
    }
  }

  gsl_Expects(python_script_engine_);
  python_script_engine_->eval(script_to_exec_);
  python_script_engine_->onSchedule(context);

  getProperty(ReloadOnScriptChange.getName(), reload_on_script_change_);
}

void ExecutePythonProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  reloadScriptIfUsingScriptFileProperty();
  if (script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  python_script_engine_->onTrigger(context, session);
}

void ExecutePythonProcessor::appendPathForImportModules() {
  std::string module_directory;
  getProperty(ModuleDirectory.getName(), module_directory);
  if (!module_directory.empty()) {
    python_script_engine_->setModulePaths(utils::StringUtils::splitAndTrimRemovingEmpty(module_directory, ","));
  }
}

void ExecutePythonProcessor::loadScriptFromFile() {
  std::ifstream file_handle(script_file_path_);
  if (!file_handle.is_open()) {
    script_to_exec_ = "";
    throw std::runtime_error("Failed to read Script File: " + script_file_path_);
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

  if (!script_file.empty()) {
    if (!script_body.empty()) {
      throw std::runtime_error("Only one of Script File or Script Body may be used");
    }
    script_file_path_ = script_file;
    loadScriptFromFile();
    last_script_write_time_ = utils::file::last_write_time(script_file_path_);
    return;
  }
  script_to_exec_ = script_body;
}

void ExecutePythonProcessor::reloadScriptIfUsingScriptFileProperty() {
  if (script_file_path_.empty() || !reload_on_script_change_) {
    return;
  }
  auto file_write_time = utils::file::last_write_time(script_file_path_);
  if (file_write_time != last_script_write_time_) {
    logger_->log_debug("Script file has changed since last time, reloading...");
    loadScriptFromFile();
    last_script_write_time_ = file_write_time;
    python_script_engine_->eval(script_to_exec_);
  }
}

std::unique_ptr<PythonScriptEngine> ExecutePythonProcessor::createScriptEngine() {
  auto engine = std::make_unique<PythonScriptEngine>();

  python_logger_ = core::logging::LoggerFactory<ExecutePythonProcessor>::getAliasedLogger(getName());
  engine->bind("log", python_logger_);
  engine->bind("REL_SUCCESS", Success);
  engine->bind("REL_FAILURE", Failure);

  return engine;
}

REGISTER_RESOURCE(ExecutePythonProcessor, Processor);

}  // namespace org::apache::nifi::minifi::python::processors
