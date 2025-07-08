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
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "PythonScriptEngine.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::python::processors {

class ExecutePythonProcessor : public core::ProcessorImpl {
 public:
  explicit ExecutePythonProcessor(core::ProcessorMetadata metadata)
      : ProcessorImpl(metadata),
        python_dynamic_(false) {
    python_logger_ = core::logging::LoggerFactory<ExecutePythonProcessor>::getAliasedLogger(getName(), metadata.uuid);
  }

  EXTENSIONAPI static constexpr const char* Description = "This processor is only used internally for running NiFi and MiNiFi C++ style python processors. Do not use this processor in your own "
      "flows. Move your python processors to the minifi-python directory where they will be parsed and then they can be used with filename as processor classes.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Script succeeds"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Script fails"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "Original flow file"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Original};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  bool supportsDynamicProperties() const override { return python_dynamic_; }
  bool supportsDynamicRelationships() const override { return SupportsDynamicRelationships; }
  minifi::core::annotation::Input getInputRequirement() const override { return InputRequirement; }
  bool isSingleThreaded() const override { return IsSingleThreaded; }
  ADD_GET_PROCESSOR_NAME

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

  void setScriptFilePath(std::string script_file_path) {
    script_file_path_ = script_file_path;
  }

  std::vector<core::Relationship> getPythonRelationships() const;
  void forEachLogger(const std::function<void(std::shared_ptr<core::logging::Logger>)>& callback) override;

 private:
  mutable std::mutex python_properties_mutex_;
  std::vector<core::Property> python_properties_;

  std::string description_;
  std::optional<std::string> version_;

  bool python_dynamic_;

  std::string script_to_exec_;
  std::optional<std::chrono::file_clock::time_point> last_script_write_time_;
  std::string script_file_path_;
  std::shared_ptr<core::logging::Logger> python_logger_;
  std::unique_ptr<PythonScriptEngine> python_script_engine_;
  std::optional<std::string> python_class_name_;
  std::vector<std::filesystem::path> python_paths_;
  std::string qualified_module_name_;

  void initializeScript();
  void loadScriptFromFile();
  std::unique_ptr<PythonScriptEngine> createScriptEngine();
  void initializeThroughScriptEngine();
  void reloadScriptFile();
};

}  // namespace org::apache::nifi::minifi::extensions::python::processors
