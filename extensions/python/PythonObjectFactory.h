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

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <filesystem>

#include "core/ClassLoader.h"
#include "ExecutePythonProcessor.h"
#include "utils/StringUtils.h"

enum class PythonProcessorType {
  MINIFI_TYPE,
  NIFI_TYPE
};

class PythonObjectFactory : public org::apache::nifi::minifi::core::DefaultObjectFactory<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor> {
 public:
  explicit PythonObjectFactory(std::string file, std::string class_name, PythonProcessorType python_processor_type,
    const std::vector<std::filesystem::path>& python_paths, std::string qualified_module_name)
      : file_(std::move(file)),
        class_name_(std::move(class_name)),
        python_paths_(python_paths),
        python_processor_type_(python_processor_type),
        qualified_module_name_(std::move(qualified_module_name)) {
  }

  std::unique_ptr<org::apache::nifi::minifi::core::CoreComponent> create(const std::string &name) override {
    auto obj = DefaultObjectFactory::create(name);
    auto ptr = org::apache::nifi::minifi::utils::dynamic_unique_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor>(std::move(obj));
    if (ptr == nullptr) {
      return nullptr;
    }
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      ptr->setPythonClassName(class_name_);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->setQualifiedModuleName(qualified_module_name_);
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile.name, file_);
    return ptr;
  }

  std::unique_ptr<org::apache::nifi::minifi::core::CoreComponent> create(const std::string &name, const org::apache::nifi::minifi::utils::Identifier &uuid) override {
    auto obj = DefaultObjectFactory::create(name, uuid);
    auto ptr = org::apache::nifi::minifi::utils::dynamic_unique_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor>(std::move(obj));
    if (ptr == nullptr) {
      return nullptr;
    }
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      ptr->setPythonClassName(class_name_);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->setQualifiedModuleName(qualified_module_name_);
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile.name, file_);
    return ptr;
  }

  org::apache::nifi::minifi::core::CoreComponent* createRaw(const std::string &name) override {
    auto ptr = dynamic_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor*>(DefaultObjectFactory::createRaw(name));
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      ptr->setPythonClassName(class_name_);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->setQualifiedModuleName(qualified_module_name_);
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile.name, file_);
    return dynamic_cast<org::apache::nifi::minifi::core::CoreComponent*>(ptr);
  }

  org::apache::nifi::minifi::core::CoreComponent* createRaw(const std::string &name, const org::apache::nifi::minifi::utils::Identifier &uuid) override {
    auto ptr = dynamic_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor*>(DefaultObjectFactory::createRaw(name, uuid));
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      ptr->setPythonClassName(class_name_);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->setQualifiedModuleName(qualified_module_name_);
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile.name, file_);
    return dynamic_cast<org::apache::nifi::minifi::core::CoreComponent*>(ptr);
  }

 private:
  std::string file_;
  std::string class_name_;
  std::vector<std::filesystem::path> python_paths_;
  PythonProcessorType python_processor_type_;
  std::string qualified_module_name_;
};
