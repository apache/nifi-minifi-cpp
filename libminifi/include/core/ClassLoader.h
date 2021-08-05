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

#ifndef LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_
#define LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_

#include <utility>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "io/BufferStream.h"
#include "ObjectFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

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
   * Constructor.
   */
  explicit ClassLoader(const std::string& name = "/");

  /**
   * Retrieves a class loader
   * @param name name of class loader
   * @return class loader reference
   */
  ClassLoader& getClassLoader(const std::string& child_name);

  /**
   * Register a class with the give ProcessorFactory
   */
  void registerClass(const std::string &clazz, std::unique_ptr<ObjectFactory> factory) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.find(clazz) != loaded_factories_.end()) {
      logger_->log_error("Class '%s' is already registered at '%s'", clazz, name_);
      return;
    }
    logger_->log_error("Registering class '%s' at '%s'", clazz, name_);
    loaded_factories_.insert(std::make_pair(clazz, std::move(factory)));
  }

  void unregisterClass(const std::string& clazz) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.erase(clazz) == 0) {
      logger_->log_error("Could not unregister non-registered class '%s' at '%s'", clazz, name_);
      return;
    } else {
      logger_->log_error("Unregistered class '%s' at '%s'", clazz, name_);
    }
  }

  std::vector<std::string> getClasses(const std::string &group) const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    std::vector<std::string> classes;
    for (const auto& child_loader : class_loaders_) {
      for (auto&& clazz : child_loader.second.getClasses(group)) {
        classes.push_back(std::move(clazz));
      }
    }
    for (const auto& factory : loaded_factories_) {
      if (factory.second->getGroupName() == group) {
        classes.push_back(factory.second->getClassName());
      }
    }
    return classes;
  }

  std::optional<std::string> getGroupForClass(const std::string &class_name) const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    for (const auto& child_loader : class_loaders_) {
      std::optional<std::string> group = child_loader.second.getGroupForClass(class_name);
      if (group) {
        return group;
      }
    }
    auto factory = loaded_factories_.find(class_name);
    if (factory != loaded_factories_.end()) {
      return factory->second->getGroupName();
    }
    return {};
  }

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, const std::string &name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, const utils::Identifier &uuid);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string &class_name, const std::string &name);

 protected:
  std::map<std::string, std::unique_ptr<ObjectFactory>> loaded_factories_;

  std::map<std::string, ClassLoader> class_loaders_;

  mutable std::mutex internal_mutex_;

  std::shared_ptr<logging::Logger> logger_;

  std::string name_;
};

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate<T>(class_name, name)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(name);
    return std::dynamic_pointer_cast<T>(obj);
  }
  return nullptr;
}

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, const utils::Identifier &uuid) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate<T>(class_name, uuid)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(class_name, uuid);
    return std::dynamic_pointer_cast<T>(obj);
  }
  return nullptr;
}

template<class T>
T *ClassLoader::instantiateRaw(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto* result = child_loader.second.instantiateRaw<T>(class_name, name)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->createRaw(name);
    return dynamic_cast<T*>(obj);
  }
  return nullptr;
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_
