/**
 * @file ExecutePythonProcessor.h
 * ExecutePythonProcessor class declaration
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

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>

#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "PythonScriptEngine.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::python::processors {

class ExecutePythonProcessor : public core::ProcessorImpl {
 public:
  explicit ExecutePythonProcessor(std::string_view name, const utils::Identifier &uuid = {})
      : ProcessorImpl(name, uuid),
        processor_initialized_(false),
        python_dynamic_(false),
        reload_on_script_change_(true) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Executes a script given the flow file and a process session. "
      "The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as "
      "any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context "
      "and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
      "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
      "concurrent tasks to 1. The python script files are expected to contain `describe(procesor)` and `onTrigger(context, session)`.";

  EXTENSIONAPI static constexpr auto ScriptFile = core::PropertyDefinitionBuilder<>::createProperty("Script File")
      .withDescription("Path to script file to execute. Only one of Script File or Script Body may be used")
      .build();
  EXTENSIONAPI static constexpr auto ScriptBody = core::PropertyDefinitionBuilder<>::createProperty("Script Body")
      .withDescription("Script to execute. Only one of Script File or Script Body may be used")
      .build();
  EXTENSIONAPI static constexpr auto ModuleDirectory = core::PropertyDefinitionBuilder<>::createProperty("Module Directory")
      .withDescription("Comma-separated list of paths to files and/or directories which contain modules required by the script")
      .build();
  EXTENSIONAPI static constexpr auto ReloadOnScriptChange = core::PropertyDefinitionBuilder<>::createProperty("Reload on Script Change")
      .withDescription("If true and Script File property is used, then script file will be reloaded if it has changed, otherwise the first loaded version will be used at all times.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ScriptFile,
      ScriptBody,
      ModuleDirectory,
      ReloadOnScriptChange
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Script succeeds"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Script fails"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "Original flow file"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Original};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void setSupportsDynamicProperties() {
    python_dynamic_ = true;
  }

  void addProperty(const std::string &name, const std::string &description, const std::optional<std::string> &defaultvalue, bool required, bool el, bool sensitive,
      const std::optional<int64_t>& property_type_code, gsl::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name);

  std::vector<core::Property> getPythonProperties() const {
    std::lock_guard<std::mutex> lock(python_properties_mutex_);
    return python_properties_;
  }

  bool getPythonSupportDynamicProperties() {
    return python_dynamic_;
  }

  void setDescription(const std::string &description) {
    description_ = description;
  }

  const std::string &getDescription() const {
    return description_;
  }

  void setVersion(const std::string& version) {
    version_ = version;
  }

  const std::optional<std::string>& getVersion() const {
    return version_;
  }

  void setPythonClassName(const std::string& python_class_name) {
    python_class_name_ = python_class_name;
  }

  void setPythonPaths(const std::vector<std::filesystem::path>& python_paths) {
    python_paths_ = python_paths;
  }

  void setQualifiedModuleName(const std::string& qualified_module_name) {
    qualified_module_name_ = qualified_module_name;
  }

  std::map<std::string, core::Property> getProperties() const override;

  std::vector<core::Relationship> getPythonRelationships();

 protected:
  const core::Property* findProperty(const std::string& name) const override;

 private:
  mutable std::mutex python_properties_mutex_;
  std::vector<core::Property> python_properties_;

  std::string description_;
  std::optional<std::string> version_;

  bool processor_initialized_;
  bool python_dynamic_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecutePythonProcessor>::getLogger(uuid_);

  std::string script_to_exec_;
  bool reload_on_script_change_;
  std::optional<std::chrono::file_clock::time_point> last_script_write_time_;
  std::string script_file_path_;
  std::shared_ptr<core::logging::Logger> python_logger_;
  std::unique_ptr<PythonScriptEngine> python_script_engine_;
  std::optional<std::string> python_class_name_;
  std::vector<std::filesystem::path> python_paths_;
  std::string qualified_module_name_;

  void appendPathForImportModules();
  void loadScriptFromFile();
  void loadScript();
  void reloadScriptIfUsingScriptFileProperty();
  void initalizeThroughScriptEngine();

  std::unique_ptr<PythonScriptEngine> createScriptEngine();
};

}  // namespace org::apache::nifi::minifi::extensions::python::processors
