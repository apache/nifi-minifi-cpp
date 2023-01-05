/**
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

#include <vector>
#include <utility>
#include <algorithm>
#include <string>
#include <memory>
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"
#include "ExecutePythonProcessor.h"
#include "PythonObjectFactory.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "range/v3/algorithm.hpp"
#include "properties/Configuration.h"

namespace org::apache::nifi::minifi::extensions::python {

/**
 * Can be used to load the python processors from NiFi properties.
 */
class PythonCreator : public minifi::core::CoreComponent {
 public:
  explicit PythonCreator(std::string name, const utils::Identifier &uuid = {})
      : minifi::core::CoreComponent(std::move(name), uuid) {
  }

  ~PythonCreator() override {
    for (const auto& clazz : registered_classes_) {
      core::getClassLoader().unregisterClass(clazz);
    }
  }

  void configure(const std::shared_ptr<Configure> &configuration) override {
    python::PythonScriptEngine::initialize();

    auto engine = std::make_shared<python::PythonScriptEngine>();
    std::optional<std::string> pathListings = configuration ? configuration->get(minifi::Configuration::nifi_python_processor_dir) : std::nullopt;
    if (!pathListings) {
      return;
    }
    configure({pathListings.value()});

    for (const auto &path : classpaths_) {
      const auto script_name = path.stem();
      const auto package = getPackage(pathListings.value(), path.string());
      std::string class_name = script_name.string();
      std::string full_name = "org.apache.nifi.minifi.processors." + script_name.string();
      if (!package.empty()) {
        full_name = utils::StringUtils::join_pack("org.apache.nifi.minifi.processors.", package, ".", script_name.string());
        class_name = full_name;
      }
      core::getClassLoader().registerClass(class_name, std::make_unique<PythonObjectFactory>(path.string(), class_name));
      registered_classes_.push_back(class_name);
      try {
        registerScriptDescription(class_name, full_name, path, script_name.string());
      } catch (const std::exception &err) {
        logger_->log_error("Cannot load %s: %s", script_name.string(), err.what());
      }
    }
  }

 private:
  void registerScriptDescription(const std::string& class_name, const std::string& full_name, const std::filesystem::path& path, const std::string& script_name) {
    auto processor = core::ClassLoader::getDefaultClassLoader().instantiate<python::processors::ExecutePythonProcessor>(class_name, utils::IdGenerator::getIdGenerator()->generate());
    if (!processor) {
      logger_->log_error("Couldn't instantiate '%s' python processor", class_name);
      return;
    }
    processor->initialize();
    minifi::BundleDetails details;
    details.artifact = path.filename().string();
    details.version = minifi::AgentBuild::VERSION;
    details.group = "python";

    minifi::ClassDescription description{
      .type_ = ResourceType::Processor,
      .short_name_ = script_name,
      .full_name_ = full_name,
      .description_ = processor->getDescription(),
      .class_properties_ = processor->getPythonProperties(),
      .class_relationships_ = processor->getSupportedRelationships(),
      .supports_dynamic_properties_ = processor->getPythonSupportDynamicProperties(),
      .inputRequirement_ = toString(processor->getInputRequirement()),
      .isSingleThreaded_ = processor->isSingleThreaded()};

    minifi::ExternalBuildDescription::addExternalComponent(details, description);
  }

  void configure(const std::vector<std::string> &pythonFiles) {
    std::vector<std::string> pathOrFiles;
    for (const auto &path : pythonFiles) {
      const auto vec = utils::StringUtils::split(path, ",");
      pathOrFiles.insert(pathOrFiles.end(), vec.begin(), vec.end());
    }

    for (const auto &path : pathOrFiles) {
      utils::file::addFilesMatchingExtension(logger_, path, ".py", classpaths_);
    }
  }

  std::string getPackage(const std::string &basePath, const std::string &pythonscript) {
    if (!minifi::utils::StringUtils::startsWith(pythonscript, basePath)) {
      return "";
    }
    const auto python_package_path = std::filesystem::relative(pythonscript, basePath).parent_path();
    std::vector<std::string> path_elements;
    path_elements.reserve(std::distance(python_package_path.begin(), python_package_path.end()));
    std::transform(python_package_path.begin(), python_package_path.end(), std::back_inserter(path_elements), [](const auto& path) { return path.string(); });
    std::string python_package = minifi::utils::StringUtils::join(".", path_elements);
    if (python_package.length() > 1 && python_package.at(0) == '.') {
      python_package = python_package.substr(1);
    }
    ranges::transform(python_package, python_package.begin(), ::tolower);
    return python_package;
  }

  std::vector<std::string> registered_classes_;
  std::vector<std::filesystem::path> classpaths_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PythonCreator>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::python
