// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
#include "core/ProcessorFactoryImpl.h"

enum class PythonProcessorType {
  MINIFI_TYPE,
  NIFI_TYPE
};

class PythonObjectFactory : public org::apache::nifi::minifi::core::ProcessorFactoryImpl<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor> {
 public:
  explicit PythonObjectFactory(std::string file, std::string class_name, PythonProcessorType python_processor_type,
    const std::vector<std::filesystem::path>& python_paths, std::string qualified_module_name)
      : file_(std::move(file)),
        class_name_(std::move(class_name)),
        python_paths_(python_paths),
        python_processor_type_(python_processor_type),
        qualified_module_name_(std::move(qualified_module_name)) {
  }

  std::unique_ptr<org::apache::nifi::minifi::core::ProcessorApi> create(org::apache::nifi::minifi::core::ProcessorMetadata metadata) override {
    auto obj = ProcessorFactoryImpl::create(metadata);
    auto ptr = org::apache::nifi::minifi::utils::dynamic_unique_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor>(std::move(obj));
    if (ptr == nullptr) {
      return nullptr;
    }
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      ptr->setPythonClassName(class_name_);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->setQualifiedModuleName(qualified_module_name_);
    ptr->setScriptFilePath(file_);
    return ptr;
  }

 private:
  std::string file_;
  std::string class_name_;
  std::vector<std::filesystem::path> python_paths_;
  PythonProcessorType python_processor_type_;
  std::string qualified_module_name_;
};
