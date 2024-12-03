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

#include <utility>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <memory>

#include "Core.h"
#include "ObjectFactory.h"

namespace org::apache::nifi::minifi::core {

#define RESOURCE_FAILURE -1

#define RESOURCE_SUCCESS 1

#ifdef WIN32
#define RTLD_LAZY   0
#define RTLD_NOW    0

#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL  (1 << 2)
#endif

/**
 * Processor class loader that accepts
 * a variety of mechanisms to load in shared
 * objects.
 */
class ClassLoader {
 public:
  static ClassLoader &getDefaultClassLoader();

  /**
   * Retrieves a class loader
   * @param name name of class loader
   * @return class loader reference
   */
  virtual ClassLoader& getClassLoader(const std::string& child_name) = 0;

  /**
   * Register a class with the give ProcessorFactory
   */
  virtual void registerClass(const std::string &clazz, std::unique_ptr<ObjectFactory> factory) = 0;

  virtual void unregisterClass(const std::string& clazz) = 0;

  virtual std::optional<std::string> getGroupForClass(const std::string &class_name) const = 0;

  virtual std::unique_ptr<CoreComponent> instantiate(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) = 0;

  virtual std::unique_ptr<CoreComponent> instantiate(const std::string &class_name, const utils::Identifier &uuid, std::function<bool(CoreComponent*)> filter) = 0;

  virtual CoreComponent* instantiateRaw(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) = 0;

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::unique_ptr<T> instantiate(const std::string &class_name, const std::string &name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::unique_ptr<T> instantiate(const std::string &class_name, const utils::Identifier &uuid);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string &class_name, const std::string &name);
};

}  // namespace org::apache::nifi::minifi::core
