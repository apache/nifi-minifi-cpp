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
#include <unordered_map>
#include <unordered_set>

#include "agent/agent_docs.h"
#include "core/Core.h"
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

  explicit ClassLoader(std::string name = "/");

  ClassLoader& getClassLoader(const std::string& child_name);

  void registerClass(const std::string &clazz, std::unique_ptr<ObjectFactory> factory, const ResourceType resource_type) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.contains(clazz)) {
      logger_->log_error("Class '{}' is already registered at '{}'", clazz, name_);
      return;
    }
    logger_->log_trace("Registering class '{}' at '{}'", clazz, name_);
    loaded_factories_.insert(std::make_pair(clazz, std::move(factory)));
    type_map_[resource_type].insert(clazz);
  }

  void unregisterClass(const std::string& clazz, const ResourceType resource_type) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.erase(clazz) == 0) {
      logger_->log_error("Could not unregister non-registered class '{}' at '{}'", clazz, name_);
      return;
    } else {
      logger_->log_trace("Unregistered class '{}' at '{}'", clazz, name_);
    }
    type_map_[resource_type].erase(clazz);
  }

  std::optional<std::string> getGroupForClass(const std::string &class_name) const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    for (const auto& child_loader : class_loaders_) {
      std::optional<std::string> group = child_loader.second.getGroupForClass(class_name);
      if (group) {
        return group;
      }
    }
    if (const auto factory = loaded_factories_.find(class_name); factory != loaded_factories_.end()) {
      return factory->second->getGroupName();
    }
    return {};
  }

  std::unordered_set<std::string> getAll(const ResourceType type) {
    std::unordered_set<std::string> type_map = type_map_[type];
    for (auto& [_, class_loader] : class_loaders_) {
      auto class_loader_type_name = class_loader.getAll(type);
      type_map.insert(class_loader_type_name.begin(), class_loader_type_name.end());
    }
    return type_map;
  }

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param name name of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::unique_ptr<T> instantiate(const std::string& class_name, const std::string& name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::unique_ptr<T> instantiate(const std::string& class_name, const utils::Identifier& uuid);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param name name of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string& class_name, const std::string& name);

 protected:
  std::map<std::string, std::unique_ptr<ObjectFactory>> loaded_factories_;

  std::map<std::string, ClassLoader> class_loaders_;

  mutable std::mutex internal_mutex_;

  std::shared_ptr<logging::Logger> logger_;

  std::unordered_map<ResourceType, std::unordered_set<std::string>> type_map_;
  std::string name_;
};

template<class T>
std::unique_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name) {
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
    return utils::dynamic_unique_cast<T>(std::move(obj));
  }
  return nullptr;
}

template<class T>
std::unique_ptr<T> ClassLoader::instantiate(const std::string &class_name, const utils::Identifier &uuid) {
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
    return utils::dynamic_unique_cast<T>(std::move(obj));
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

}  // namespace org::apache::nifi::minifi::core
