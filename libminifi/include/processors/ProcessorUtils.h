// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <core/ConfigurableComponentImpl.h>
#include <core/Processor.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace org::apache::nifi::minifi::processors {

class ProcessorUtils {
 public:
  ProcessorUtils() = delete;

  static inline std::unique_ptr<core::Processor> createProcessor(const std::string &class_short, const std::string &fullclass, const std::string &object_name, const utils::Identifier &uuid) {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(class_short, object_name, uuid);
    if (ptr == nullptr) {
      ptr = core::ClassLoader::getDefaultClassLoader().instantiate(fullclass, object_name, uuid);
    }
    if (ptr == nullptr) {
      return nullptr;
    }

    auto returnPtr = utils::dynamic_unique_cast<core::Processor>(std::move(ptr));
    if (!returnPtr) {
      throw std::runtime_error("Invalid return from the classloader");
    }

    returnPtr->initialize();
    return returnPtr;
  }
};

}  // namespace org::apache::nifi::minifi::processors
