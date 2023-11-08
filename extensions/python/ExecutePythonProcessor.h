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
#include <utility>
#include <vector>

#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "PythonScriptEngine.h"
#include "PythonScriptEngine.h"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

namespace org::apache::nifi::minifi::extensions::python::processors {

class ExecutePythonProcessor : public core::Processor {
 public:
  explicit ExecutePythonProcessor(std::string_view name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
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
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 4>{
      ScriptFile,
      ScriptBody,
      ModuleDirectory,
      ReloadOnScriptChange
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Script succeeds"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Script fails"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onScheduleSharedPtr(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTriggerSharedPtr(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  void setSupportsDynamicProperties() {
    python_dynamic_ = true;
  }

  void addProperty(const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el) {
    python_properties_.emplace_back(
        core::PropertyDefinitionBuilder<>::createProperty(name).withDefaultValue(defaultvalue).withDescription(description).isRequired(required).supportsExpressionLanguage(el).build());
  }

  const std::vector<core::Property> &getPythonProperties() const {
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

 private:
  std::vector<core::Property> python_properties_;

  std::string description_;

  bool processor_initialized_;
  bool python_dynamic_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecutePythonProcessor>::getLogger(uuid_);

  std::string script_to_exec_;
  bool reload_on_script_change_;
  std::optional<std::chrono::file_clock::time_point> last_script_write_time_;
  std::string script_file_path_;
  std::shared_ptr<core::logging::Logger> python_logger_;
  std::unique_ptr<PythonScriptEngine> python_script_engine_;

  void appendPathForImportModules();
  void loadScriptFromFile();
  void loadScript();
  void reloadScriptIfUsingScriptFileProperty();
  void initalizeThroughScriptEngine();

  std::unique_ptr<PythonScriptEngine> createScriptEngine();
};

}  // namespace org::apache::nifi::minifi::extensions::python::processors

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility pop
#endif
