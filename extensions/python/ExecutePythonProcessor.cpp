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

#include "ExecutePythonProcessor.h"

#include <set>
#include <stdexcept>
#include <utility>

#include "PythonConfigState.h"
#include "controllers/SSLContextService.h"
#include "core/Resource.h"
#include "range/v3/algorithm/find_if.hpp"
#include "range/v3/range/conversion.hpp"
#include "types/PyLogger.h"
#include "types/PyRelationship.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::extensions::python::processors {

void ExecutePythonProcessor::initialize() {
  if (getSupportedProperties().empty()) {
    setSupportedProperties(Properties);
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
  } catch (const PythonScriptWarning&) {
    throw;
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

  reload_on_script_change_ = utils::parseBoolProperty(context, ReloadOnScriptChange);
}

void ExecutePythonProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  reloadScriptIfUsingScriptFileProperty();
  if (script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  python_script_engine_->onTrigger(context, session);
}

void ExecutePythonProcessor::appendPathForImportModules() const {
  std::string module_directory = getProperty(ModuleDirectory.name).value_or("");
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
  std::string script_file = getProperty(ScriptFile.name).value_or("");
  std::string script_body = getProperty(ScriptBody.name).value_or("");
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

namespace {
enum class PropertyValidatorCode : int64_t {
  INTEGER = 0,
  LONG = 1,
  BOOLEAN = 2,
  DATA_SIZE = 3,
  TIME_PERIOD = 4,
  NON_BLANK = 5,
  PORT = 6
};

const core::PropertyValidator& translateCodeToPropertyValidator(const PropertyValidatorCode& code) {
  switch (code) {
    case PropertyValidatorCode::INTEGER:  // NOLINT(*-branch-clone)
      return core::StandardPropertyValidators::INTEGER_VALIDATOR;
    case PropertyValidatorCode::LONG:
      return core::StandardPropertyValidators::INTEGER_VALIDATOR;
    case PropertyValidatorCode::BOOLEAN:
      return core::StandardPropertyValidators::BOOLEAN_VALIDATOR;
    case PropertyValidatorCode::DATA_SIZE:
      return core::StandardPropertyValidators::DATA_SIZE_VALIDATOR;
    case PropertyValidatorCode::TIME_PERIOD:
      return core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR;
    case PropertyValidatorCode::NON_BLANK:
      return core::StandardPropertyValidators::NON_BLANK_VALIDATOR;
    case PropertyValidatorCode::PORT:
      return core::StandardPropertyValidators::PORT_VALIDATOR;
    default:
      throw std::invalid_argument("Unknown PropertyValidatorCode");
  }
}
}  // namespace

void ExecutePythonProcessor::addProperty(const std::string &name, const std::string &description, const std::optional<std::string> &defaultvalue, bool required, bool el, bool sensitive,
    const std::optional<int64_t>& property_type_code, gsl::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name) {
  auto builder = core::PropertyDefinitionBuilder<>::createProperty(name).withDescription(description).isRequired(required).supportsExpressionLanguage(el).isSensitive(sensitive);
  if (defaultvalue) {
    builder.withDefaultValue(*defaultvalue);
  }
  if (property_type_code) {
    builder.withValidator(translateCodeToPropertyValidator(static_cast<PropertyValidatorCode>(*property_type_code)));
  }
  if (controller_service_type_name && *controller_service_type_name == "SSLContextService") {
    builder.withAllowedTypes<controllers::SSLContextService>();
  }
  const auto property_definition = builder.build();

  core::Property property{property_definition};
  std::vector<std::string> allowed_values{allowable_values.begin(), allowable_values.end()};
  property.setAllowedValues(std::move(allowed_values));

  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  python_properties_.emplace_back(property);
}

nonstd::expected<std::string, std::error_code> ExecutePythonProcessor::getProperty(std::string_view name) const {
  if (auto non_python_property = ConfigurableComponentImpl::getProperty(name)) {
    return *non_python_property;
  }
  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  auto it = ranges::find_if(python_properties_, [&name](const auto& item){
    return item.getName() == name;
  });
  if (it != python_properties_.end()) {
    return it->getValue() | utils::transform([](const std::string_view value_view) -> std::string { return std::string{value_view}; });
  }
  return nonstd::make_unexpected(core::PropertyErrorCode::NotSupportedProperty);
}

nonstd::expected<void, std::error_code> ExecutePythonProcessor::setProperty(std::string_view name, std::string value) {
  auto set_non_python_property = ConfigurableComponentImpl::setProperty(name, value);
  if (set_non_python_property || set_non_python_property.error() != core::PropertyErrorCode::NotSupportedProperty) {
    return set_non_python_property;
  }
  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  auto it = ranges::find_if(python_properties_, [&name](const auto& item){
    return item.getName() == name;
  });
  if (it != python_properties_.end()) {
    return it->setValue(std::move(value));
  }
  return nonstd::make_unexpected(core::PropertyErrorCode::NotSupportedProperty);
}

nonstd::expected<core::Property, std::error_code> ExecutePythonProcessor::getSupportedProperty(std::string_view name) const {
  if (const auto non_python_property = ConfigurableComponentImpl::getSupportedProperty(name)) {
    return *non_python_property;
  }
  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  auto it = ranges::find_if(python_properties_, [&name](const auto& item){
    return item.getName() == name;
  });
  if (it != python_properties_.end()) {
    return *it;
  }
  return nonstd::make_unexpected(core::PropertyErrorCode::NotSupportedProperty);
}

std::map<std::string, core::Property, std::less<>> ExecutePythonProcessor::getSupportedProperties() const {
  auto result = ConfigurableComponentImpl::getSupportedProperties();

  std::lock_guard<std::mutex> lock(python_properties_mutex_);

  for (const auto &property : python_properties_) {
    result.insert({ property.getName(), property });
  }

  return result;
}

std::vector<core::Relationship> ExecutePythonProcessor::getPythonRelationships() const {
  auto relationships = getSupportedRelationships();
  auto custom_relationships = python_script_engine_->getCustomPythonRelationships();
  relationships.reserve(relationships.size() + std::distance(custom_relationships.begin(), custom_relationships.end()));
  relationships.insert(relationships.end(), custom_relationships.begin(), custom_relationships.end());
  return relationships;
}

REGISTER_RESOURCE(ExecutePythonProcessor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::python::processors
