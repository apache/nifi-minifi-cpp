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

#include <set>
#include <stdexcept>
#include <utility>

#include "ExecutePythonProcessor.h"
#include "PythonConfigState.h"
#include "types/PyRelationship.h"
#include "types/PyLogger.h"
#include "controllers/SSLContextService.h"

#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Resource.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/algorithm/find_if.hpp"

namespace org::apache::nifi::minifi::extensions::python::processors {

void ExecutePythonProcessor::initialize() {
  if (getProperties().empty()) {
    setSupportedProperties(Properties);
    setAcceptAllProperties();
    setSupportedRelationships(Relationships);
  }

  if (processor_initialized_) {
    logger_->log_debug("Processor has already been initialized, returning...");
    return;
  }

  try {
    loadScript();
  } catch(const std::runtime_error&) {
    return;
  }

  // In case of native python processors we require initialization before onSchedule
  // so that we can provide manifest of processor identity on C2
  python_script_engine_ = createScriptEngine();
  initalizeThroughScriptEngine();
}

void ExecutePythonProcessor::initalizeThroughScriptEngine() {
  try {
    appendPathForImportModules();
    python_script_engine_->appendModulePaths(python_paths_);
    python_script_engine_->setModuleAttributes(qualified_module_name_);
    python_script_engine_->eval(script_to_exec_);
    if (python_class_name_) {
      python_script_engine_->initializeProcessorObject(*python_class_name_);
    }
    python_script_engine_->describe(this);
    python_script_engine_->onInitialize(this);
    processor_initialized_ = true;
  } catch (const std::exception& e) {
    std::string python_processor_name = python_class_name_ ? *python_class_name_ : script_file_path_;
    logger_->log_error("Failed to initialize python processor '{}' due to error: {}", python_processor_name, e.what());
    throw;
  }
}

void ExecutePythonProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& /*sessionFactory*/) {
  addAutoTerminatedRelationship(Original);
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

  getProperty(ReloadOnScriptChange, reload_on_script_change_);
}

void ExecutePythonProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  reloadScriptIfUsingScriptFileProperty();
  if (script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  python_script_engine_->onTrigger(context, session);
}

void ExecutePythonProcessor::appendPathForImportModules() {
  std::string module_directory;
  getProperty(ModuleDirectory, module_directory);
  if (!module_directory.empty()) {
    python_script_engine_->appendModulePaths(utils::string::splitAndTrimRemovingEmpty(module_directory, ",") | ranges::to<std::vector<std::filesystem::path>>());
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
  getProperty(ScriptFile, script_file);
  getProperty(ScriptBody, script_body);
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
  engine->initialize(Success, Failure, Original, python_logger_);

  return engine;
}

void ExecutePythonProcessor::addProperty(const std::string &name, const std::string &description, const std::optional<std::string> &defaultvalue, bool required, bool el, bool sensitive,
    const std::optional<int64_t>& property_type_code, gsl::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name) {
  auto builder = core::PropertyDefinitionBuilder<>::createProperty(name).withDescription(description).isRequired(required).supportsExpressionLanguage(el).isSensitive(sensitive);
  if (defaultvalue) {
    builder.withDefaultValue(*defaultvalue);
  }
  if (property_type_code) {
    builder.withPropertyType(core::StandardPropertyTypes::translateCodeToPropertyType(static_cast<core::StandardPropertyTypes::PropertyTypeCode>(*property_type_code)));
  }
  if (controller_service_type_name && *controller_service_type_name == "SSLContextService") {
    builder.withAllowedTypes<controllers::SSLContextService>();
  }
  const auto property_definition = builder.build();

  core::Property property{property_definition};
  property.setAllowedValues(allowable_values, *property_definition.type);

  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  python_properties_.emplace_back(property);
}

const core::Property* ExecutePythonProcessor::findProperty(const std::string& name) const {
  if (auto prop_ptr = core::ConfigurableComponentImpl::findProperty(name)) {
    return prop_ptr;
  }

  std::lock_guard<std::mutex> lock(python_properties_mutex_);

  auto it = ranges::find_if(python_properties_, [&name](const auto& item){
    return item.getName() == name;
  });
  if (it != python_properties_.end()) {
    return &*it;
  }

  return nullptr;
}

std::map<std::string, core::Property> ExecutePythonProcessor::getProperties() const {
  auto result = ConfigurableComponentImpl::getProperties();

  std::lock_guard<std::mutex> lock(python_properties_mutex_);

  for (const auto &property : python_properties_) {
    result.insert({ property.getName(), property });
  }

  return result;
}

std::vector<core::Relationship> ExecutePythonProcessor::getPythonRelationships() {
  auto relationships = getSupportedRelationships();
  auto custom_relationships = python_script_engine_->getCustomPythonRelationships();
  relationships.reserve(relationships.size() + std::distance(custom_relationships.begin(), custom_relationships.end()));
  relationships.insert(relationships.end(), custom_relationships.begin(), custom_relationships.end());
  return relationships;
}

REGISTER_RESOURCE(ExecutePythonProcessor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::python::processors
