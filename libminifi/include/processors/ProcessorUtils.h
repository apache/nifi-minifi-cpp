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

  /**
   * Instantiates and configures a processor
   * @param class_short_name short name of the class
   * @param canonicalName full class name ( canonical name )
   * @param uuid uuid object for the processor
   */
  static inline std::unique_ptr<core::Processor> createProcessor(const std::string &class_short_name, const std::string &canonicalName, const utils::Identifier &uuid) {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(class_short_name, uuid);
    if (ptr == nullptr) {
      ptr = core::ClassLoader::getDefaultClassLoader().instantiate(canonicalName, uuid);
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
