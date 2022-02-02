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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

/**
 * Can be used to load the python processors from NiFi properties.
 */
class PythonCreator : public minifi::core::CoreComponent {
 public:
  explicit PythonCreator(const std::string &name, const utils::Identifier &uuid = {})
      : minifi::core::CoreComponent(name, uuid) {
  }

  ~PythonCreator() override {
    for (const auto& clazz : registered_classes_) {
      core::getClassLoader().unregisterClass(clazz);
    }
  }

  void configure(const std::shared_ptr<Configure> &configuration) override {
    python::PythonScriptEngine::initialize();

    auto engine = std::make_shared<python::PythonScriptEngine>();
    std::optional<std::string> pathListings = configuration ? configuration->get("nifi.python.processor.dir") : std::nullopt;
    if (!pathListings) {
      return;
    }
    configure({pathListings.value()});

    for (const auto &path : classpaths_) {
      const auto scriptname = getScriptName(path);
      const auto package = getPackage(pathListings.value(), path);
      std::string classname = scriptname;
      std::string fullname = "org.apache.nifi.minifi.processors." + scriptname;
      if (!package.empty()) {
        fullname = utils::StringUtils::join_pack("org.apache.nifi.minifi.processors.", package, ".", scriptname);
        classname = fullname;
      }
      core::getClassLoader().registerClass(classname, std::make_unique<PythonObjectFactory>(path, classname));
      registered_classes_.push_back(classname);
      try {
        registerScriptDescription(classname, fullname, path, scriptname);
      } catch (const std::exception &err) {
        logger_->log_error("Cannot load %s: %s", scriptname, err.what());
      }
    }
  }

 private:
  void registerScriptDescription(const std::string& classname, const std::string& fullname, const std::string& path, const std::string& scriptname) {
    auto processor = core::ClassLoader::getDefaultClassLoader().instantiate<python::processors::ExecutePythonProcessor>(classname, utils::IdGenerator::getIdGenerator()->generate());
    if (!processor) {
      logger_->log_error("Couldn't instantiate '%s' python processor", classname);
      return;
    }
    processor->initialize();
    minifi::BundleDetails details;
    details.artifact = getFileName(path);
    details.version = minifi::AgentBuild::VERSION;
    details.group = "python";

    minifi::ClassDescription description(fullname);
    description.dynamic_properties_ = processor->getPythonSupportDynamicProperties();
    description.inputRequirement_ = processor->getInputRequirementAsString();
    description.isSingleThreaded_ = processor->isSingleThreaded();
    auto properties = processor->getPythonProperties();

    minifi::AgentDocs::putDescription(scriptname, processor->getDescription());
    for (const auto &prop : properties) {
      description.class_properties_.insert(std::make_pair(prop.getName(), prop));
    }

    for (const auto &rel : processor->getSupportedRelationships()) {
      description.class_relationships_.push_back(rel);
    }
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
    std::vector<std::string> path_elements{python_package_path.begin(), python_package_path.end()};
    std::string python_package = minifi::utils::StringUtils::join(".", path_elements);
    if (python_package.length() > 1 && python_package.at(0) == '.') {
      python_package = python_package.substr(1);
    }
    ranges::transform(python_package, python_package.begin(), ::tolower);
    return python_package;
  }

  std::string getPath(const std::string &pythonscript) {
    return std::filesystem::path(pythonscript).parent_path().string();
  }

  std::string getFileName(const std::string &pythonscript) {
    return std::filesystem::path(pythonscript).filename().string();
  }

  std::string getScriptName(const std::string &pythonscript) {
    return std::filesystem::path(pythonscript).stem().string();
  }

  std::vector<std::string> registered_classes_;
  std::vector<std::string> classpaths_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PythonCreator>::getLogger();
};

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
