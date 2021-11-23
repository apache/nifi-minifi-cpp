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

#include <core/ConfigurableComponent.h>
#include <core/Processor.h>

#include <memory>
#include <string>
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
   * @param stream_factory stream factory to be set onto the processor
   */
  static inline std::unique_ptr<core::Processor> createProcessor(const std::string &class_short_name, const std::string &canonicalName, const utils::Identifier &uuid,
                                                                 const std::shared_ptr<minifi::io::StreamFactory> &stream_factory) {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(class_short_name, uuid);
    // fallback to the canonical name if the short name does not work, then attempt JNI bindings if they exist
    if (ptr == nullptr) {
      ptr = core::ClassLoader::getDefaultClassLoader().instantiate(canonicalName, uuid);
      if (ptr == nullptr) {
        ptr = core::ClassLoader::getDefaultClassLoader().instantiate("ExecuteJavaClass", uuid);
        if (ptr != nullptr) {
          if (dynamic_cast<core::Processor*>(ptr.get()) == nullptr) {
            throw std::runtime_error("Invalid return from the classloader");
          }
          auto processor = std::unique_ptr<core::Processor>{dynamic_cast<core::Processor*>(ptr.release())};
          processor->initialize();
          processor->setProperty("NiFi Processor", canonicalName);
          processor->setStreamFactory(stream_factory);
          return processor;
        }
      }
    }
    if (ptr == nullptr) {
      return nullptr;
    }
    if (dynamic_cast<core::Processor*>(ptr.get()) == nullptr) {
      throw std::runtime_error("Invalid return from the classloader");
    }

    auto returnPtr = std::unique_ptr<core::Processor>{dynamic_cast<core::Processor*>(ptr.release())};
    returnPtr->initialize();
    returnPtr->setStreamFactory(stream_factory);
    return returnPtr;
  }
};

}  // namespace org::apache::nifi::minifi::processors
