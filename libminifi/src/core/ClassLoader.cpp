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

#include "core/ClassLoader.h"

#include <memory>
#include <string>

#include "core/logging/LoggerFactory.h"
#include "range/v3/action/sort.hpp"
#include "range/v3/action/unique.hpp"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class ClassLoaderImpl : public ClassLoader {
 public:
  explicit ClassLoaderImpl(const std::string& name = "/");

  ClassLoader& getClassLoader(const std::string& child_name) override;

  void registerClass(const std::string &clazz, std::unique_ptr<ObjectFactory> factory) override;

  void unregisterClass(const std::string& clazz) override;

  std::optional<std::string> getGroupForClass(const std::string &class_name) const override;

  std::unique_ptr<CoreComponent> instantiate(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) override;

  std::unique_ptr<CoreComponent> instantiate(const std::string &class_name, const utils::Identifier &uuid, std::function<bool(CoreComponent*)> filter) override;

  CoreComponent* instantiateRaw(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) override;

 private:
  std::map<std::string, std::unique_ptr<ObjectFactory>> loaded_factories_;

  std::map<std::string, ClassLoaderImpl> class_loaders_;

  mutable std::mutex internal_mutex_;

  std::shared_ptr<logging::Logger> logger_;

  std::string name_;
};

ClassLoaderImpl::ClassLoaderImpl(const std::string& name)
  : logger_(logging::LoggerFactory<ClassLoader>::getLogger()), name_(name) {}

ClassLoader &ClassLoader::getDefaultClassLoader() {
  static ClassLoaderImpl ret;
  // populate ret
  return ret;
}

ClassLoader& ClassLoaderImpl::getClassLoader(const std::string& child_name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto it = class_loaders_.find(child_name);
  if (it != class_loaders_.end()) {
    return it->second;
  }
  std::string full_name = [&] {
    if (name_ == "/") {
      return "/" + child_name;
    }
    return name_ + "/" + child_name;
  }();
  ClassLoaderImpl& child = class_loaders_[child_name];
  child.name_ = std::move(full_name);
  return child;
}

void ClassLoaderImpl::registerClass(const std::string &clazz, std::unique_ptr<ObjectFactory> factory) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.find(clazz) != loaded_factories_.end()) {
      logger_->log_error("Class '{}' is already registered at '{}'", clazz, name_);
      return;
    }
    logger_->log_trace("Registering class '{}' at '{}'", clazz, name_);
    loaded_factories_.insert(std::make_pair(clazz, std::move(factory)));
  }

void ClassLoaderImpl::unregisterClass(const std::string& clazz) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  if (loaded_factories_.erase(clazz) == 0) {
    logger_->log_error("Could not unregister non-registered class '{}' at '{}'", clazz, name_);
    return;
  } else {
    logger_->log_trace("Unregistered class '{}' at '{}'", clazz, name_);
  }
}

std::optional<std::string> ClassLoaderImpl::getGroupForClass(const std::string &class_name) const {
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

std::unique_ptr<CoreComponent> ClassLoaderImpl::instantiate(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate(class_name, name, filter)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(name);
    if (filter(obj.get())) {
      return obj;
    }
  }
  return nullptr;
}

std::unique_ptr<CoreComponent> ClassLoaderImpl::instantiate(const std::string &class_name, const utils::Identifier &uuid, std::function<bool(CoreComponent*)> filter) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate(class_name, uuid, filter)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(class_name, uuid);
    if (filter(obj.get())) {
      return obj;
    }
  }
  return nullptr;
}

CoreComponent* ClassLoaderImpl::instantiateRaw(const std::string &class_name, const std::string &name, std::function<bool(CoreComponent*)> filter) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto* result = child_loader.second.instantiateRaw(class_name, name, filter)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->createRaw(name);
    if (filter(obj)) {
      return obj;
    }
  }
  return nullptr;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
