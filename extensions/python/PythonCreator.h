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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ExecutePythonProcessor.h"
#include "PythonConfigState.h"
#include "PythonDependencyInstaller.h"
#include "PythonObjectFactory.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/agent/agent_version.h"
#include "range/v3/algorithm.hpp"
#include "range/v3/view/filter.hpp"
#include "utils/Environment.h"
#include "utils/Locations.h"
#include "utils/StringUtils.h"
#include "utils/file/FilePattern.h"
#include "utils/file/FileUtils.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"
#include "utils/ExtensionInitUtils.h"

namespace org::apache::nifi::minifi::extensions::python {

class DummyProcessorDescriptor : public core::ProcessorDescriptor {
 public:
  ~DummyProcessorDescriptor() override = default;

  void setSupportedRelationships(std::span<const core::RelationshipDefinition> /*relationships*/) override {}
  void setSupportedProperties(std::span<const core::PropertyReference> /*properties*/) override {}
  void setSupportedProperties(std::span<const core::Property> /*properties*/) override {}
};

class DummyLogger : public core::logging::Logger {
 public:
  void set_max_log_size(int /*size*/) override {}
  void log_string(core::logging::LOG_LEVEL /*level*/, std::string /*str*/) override {}
  bool should_log(core::logging::LOG_LEVEL /*level*/) override {
    return false;
  }
  [[nodiscard]] core::logging::LOG_LEVEL level() const override {
    return core::logging::LOG_LEVEL::off;
  }

  ~DummyLogger() override = default;
};

/**
 * Can be used to load the python processors from NiFi properties.
 */
class PythonCreator : public minifi::core::CoreComponentImpl {
 public:
  explicit PythonCreator(const std::string_view name, const utils::Identifier &uuid = {})
      : minifi::core::CoreComponentImpl(name, uuid) {
  }

  ~PythonCreator() override {
    for (const auto& [clazz, factory] : registered_classes_) {
      core::getClassLoader().unregisterClass(clazz);
    }
    registered_classes_.clear();
  }

  void configure(const std::shared_ptr<Configure>& configuration) override {
    configure([&] (std::string_view key) -> std::optional<std::string> {
      return configuration->get(std::string{key});
    });
  }

  void configure(const minifi::utils::ConfigReader& config_reader) {
    std::optional<std::string> pathListings = config_reader(minifi::Configuration::nifi_python_processor_dir);
    if (!pathListings) {
      return;
    }
    configure({pathListings.value()});

    PythonDependencyInstaller dependency_installer(config_reader);
    dependency_installer.installDependencies(classpaths_);

    auto python_lib_path = getPythonLibPath(config_reader);
    for (const auto &path : classpaths_) {
      const auto script_name = path.stem();
      const auto package = getPackage(pathListings.value(), path.string());
      std::string class_name = script_name.string();
      std::string full_name = "org.apache.nifi.minifi.processors." + script_name.string();
      if (!package.empty()) {
        full_name = utils::string::join_pack("org.apache.nifi.minifi.processors.", package, ".", script_name.string());
        class_name = full_name;
      }

      const std::string qualified_module_name_prefix = "org.apache.nifi.minifi.processors.";
      std::string qualified_module_name = full_name.substr(qualified_module_name_prefix.length());
      std::unique_ptr<PythonObjectFactory> factory;
      if (path.string().find("nifi_python_processors") != std::string::npos) {
        auto utils_path = (std::filesystem::path("nifi_python_processors") / "utils").string();
        if (path.string().find(utils_path) != std::string::npos) {
          continue;
        }
        logger_->log_info("Registering NiFi python processor: {}", class_name);
        factory = std::make_unique<PythonObjectFactory>(path.string(), script_name.string(),
          PythonProcessorType::NIFI_TYPE, std::vector<std::filesystem::path>{python_lib_path, std::filesystem::path{pathListings.value()}, path.parent_path()}, qualified_module_name);
      } else {
        logger_->log_info("Registering MiNiFi python processor: {}", class_name);
        factory = std::make_unique<PythonObjectFactory>(path.string(), script_name.string(),
          PythonProcessorType::MINIFI_TYPE, std::vector<std::filesystem::path>{python_lib_path, std::filesystem::path{pathListings.value()}}, qualified_module_name);
      }
      if (registered_classes_.emplace(class_name, factory.get()).second) {
        core::getClassLoader().registerClass(class_name, std::move(factory));
      } else {
        logger_->log_warn("Ignoring duplicate python processor at '{}' with class '{}'", path.string(), class_name);
      }
      try {
        registerScriptDescription(class_name, full_name, path, script_name.string());
      } catch (const PythonScriptWarning &warning) {
        logger_->log_info("Could not process {}.py: {} -- if this is a helper file, then this is OK", script_name, warning.what());
      } catch (const std::exception &err) {
        logger_->log_error("Could not process {}.py: {}", script_name, err.what());
      }
    }
  }

 private:
  void registerScriptDescription(const std::string& class_name, const std::string& full_name, const std::filesystem::path& path, const std::string& script_name) {
    std::unique_ptr<python::processors::ExecutePythonProcessor> processor;
    auto factory_it = registered_classes_.find(class_name);
    if (factory_it != registered_classes_.end()) {
      processor = utils::dynamic_unique_cast<python::processors::ExecutePythonProcessor>(factory_it->second->create(core::ProcessorMetadata{
        .uuid = utils::IdGenerator::getIdGenerator()->generate(),
        .name = class_name,
        .logger = std::make_shared<DummyLogger>()
      }));
    }
    if (!processor) {
      logger_->log_error("Couldn't instantiate '{}' python processor", class_name);
      return;
    }
    DummyProcessorDescriptor descriptor;
    processor->core::ProcessorImpl::initialize(descriptor);
    minifi::BundleIdentifier details;
    details.name = path.filename().string();
    details.version = processor->getVersion() && !processor->getVersion()->empty() ? *processor->getVersion() : minifi::AgentBuild::VERSION;

    minifi::ClassDescription description{
      .type_ = ResourceType::Processor,
      .short_name_ = script_name,
      .full_name_ = full_name,
      .description_ = processor->getDescription(),
      .class_properties_ = processor->getPythonProperties(),
      .class_relationships_ = processor->getPythonRelationships(),
      .supports_dynamic_properties_ = processor->supportsDynamicProperties(),
      .inputRequirement_ = toString(processor->getInputRequirement()),
      .isSingleThreaded_ = processor->isSingleThreaded()};

    minifi::ClassDescriptionRegistry::getMutableClassDescriptions()[details].processors.push_back(description);
  }

  void configure(const std::vector<std::string> &pythonFiles) {
    std::vector<std::string> pathOrFiles;
    for (const auto &path : pythonFiles) {
      const auto vec = utils::string::split(path, ",");
      pathOrFiles.insert(pathOrFiles.end(), vec.begin(), vec.end());
    }

    for (const auto &path : pathOrFiles) {
      utils::file::addFilesMatchingExtension(logger_, path, ".py", classpaths_);
    }
    classpaths_ = classpaths_
      | ranges::views::filter([] (auto& path) { return path.string().find("nifiapi") == std::string::npos && path.string().find("__init__") == std::string::npos; })
      | ranges::to<std::vector<std::filesystem::path>>();
  }

  std::string getPackage(const std::string &basePath, const std::string &pythonscript) {
    if (!minifi::utils::string::startsWith(pythonscript, basePath)) {
      return "";
    }
    const auto python_package_path = std::filesystem::relative(pythonscript, basePath).parent_path();
    std::vector<std::string> path_elements;
    path_elements.reserve(std::distance(python_package_path.begin(), python_package_path.end()));
    std::transform(python_package_path.begin(), python_package_path.end(), std::back_inserter(path_elements), [](const auto& path) { return path.string(); });
    std::string python_package = minifi::utils::string::join(".", path_elements);
    if (python_package.length() > 1 && python_package.at(0) == '.') {
      python_package = python_package.substr(1);
    }
    ranges::transform(python_package, python_package.begin(), ::tolower);
    return python_package;
  }

  std::filesystem::path getPythonLibPath(const minifi::utils::ConfigReader& config_reader) {
    const std::string pattern = [&] {
      if (const auto opt_pattern = config_reader(Configuration::nifi_extension_path)) {
        return *opt_pattern;
      };
      const auto default_extension_path = utils::getDefaultExtensionsPattern();
      logger_->log_warn("No extension path is provided in properties, using default: '{}'", default_extension_path);
      return std::string(default_extension_path);
    }();
    const auto candidates = utils::file::match(utils::file::FilePattern(pattern, [&] (std::string_view subpattern, std::string_view error_msg) {
      logger_->log_error("Error in subpattern '{}': {}", subpattern, error_msg);
    }));

    std::filesystem::path python_lib_path;
    for (const auto& candidate : candidates) {
      if (candidate.string().find("python") != std::string::npos) {
        python_lib_path = candidate.parent_path();
        break;
      }
    }

    return python_lib_path;
  }

  std::unordered_map<std::string, PythonObjectFactory*> registered_classes_;
  std::vector<std::filesystem::path> classpaths_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PythonCreator>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::python
