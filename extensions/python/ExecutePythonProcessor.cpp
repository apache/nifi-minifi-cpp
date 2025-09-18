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
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "core/Resource.h"
#include "range/v3/algorithm/find_if.hpp"
#include "range/v3/range/conversion.hpp"
#include "types/PyLogger.h"
#include "types/PyRelationship.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/PropertyErrors.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"

namespace org::apache::nifi::minifi::extensions::python::processors {

void ExecutePythonProcessor::initializeScript() {
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

void ExecutePythonProcessor::initialize() {
  initializeScript();
  std::vector<core::PropertyReference> all_properties{Properties.begin(), Properties.end()};
  ranges::transform(python_properties_, std::back_inserter(all_properties), &core::Property::getReference);
  setSupportedProperties(all_properties);
  setSupportedRelationships(Relationships);
  logger_->log_debug("Processor has been initialized.");
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
  context.addAutoTerminatedRelationship(Original);
  if (!processor_initialized_) {
    script_file_path_ = context.getProperty(ScriptFile.name).value_or("");
    script_to_exec_ = context.getProperty(ScriptBody.name).value_or("");
    module_directory_ = context.getProperty(ModuleDirectory.name).value_or("");
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
  if (!module_directory_.empty()) {
    python_script_engine_->appendModulePaths(utils::string::splitAndTrimRemovingEmpty(module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
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
  if (script_file_path_.empty() && script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  if (!script_file_path_.empty()) {
    if (!script_to_exec_.empty()) {
      throw std::runtime_error("Only one of Script File or Script Body may be used");
    }
    loadScriptFromFile();
    last_script_write_time_ = utils::file::last_write_time(script_file_path_);
  }
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
    builder.withAllowedTypes<controllers::SSLContextServiceInterface>();
  }
  const auto property_definition = builder.build();

  core::Property property{property_definition};
  std::vector<std::string> allowed_values{allowable_values.begin(), allowable_values.end()};
  property.setAllowedValues(std::move(allowed_values));

  std::lock_guard<std::mutex> lock(python_properties_mutex_);
  python_properties_.emplace_back(property);
}

std::vector<core::Relationship> ExecutePythonProcessor::getPythonRelationships() const {
  std::vector<core::Relationship> relationships{Relationships.begin(), Relationships.end()};
  auto custom_relationships = python_script_engine_->getCustomPythonRelationships();
  relationships.reserve(relationships.size() + std::distance(custom_relationships.begin(), custom_relationships.end()));
  relationships.insert(relationships.end(), custom_relationships.begin(), custom_relationships.end());
  return relationships;
}

void ExecutePythonProcessor::setLoggerCallback(const std::function<void(core::logging::LOG_LEVEL level, const std::string& message)>& callback) {
  gsl_Expects(logger_ && python_logger_);
  logger_->setLogCallback(callback);
  python_logger_->setLogCallback(callback);
}

REGISTER_RESOURCE(ExecutePythonProcessor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::python::processors
