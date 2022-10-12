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

#include "UnorderedMapKeyValueStoreService.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

const core::Property UnorderedMapKeyValueStoreService::LinkedServices(
    core::PropertyBuilder::createProperty("Linked Services")
    ->withDescription("Referenced Controller Services")
    ->build());

UnorderedMapKeyValueStoreService::UnorderedMapKeyValueStoreService(std::string name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : PersistableKeyValueStoreService(std::move(name), uuid) {
}

UnorderedMapKeyValueStoreService::UnorderedMapKeyValueStoreService(std::string name, const std::shared_ptr<Configure> &configuration)
    : PersistableKeyValueStoreService(std::move(name)) {
  setConfiguration(configuration);
}

UnorderedMapKeyValueStoreService::~UnorderedMapKeyValueStoreService() = default;

bool UnorderedMapKeyValueStoreService::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  map_[key] = value;
  return true;
}

bool UnorderedMapKeyValueStoreService::get(const std::string& key, std::string& value) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    return false;
  } else {
    value = it->second;
    return true;
  }
}

bool UnorderedMapKeyValueStoreService::get(std::unordered_map<std::string, std::string>& kvs) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  kvs = map_;
  return true;
}

bool UnorderedMapKeyValueStoreService::remove(const std::string& key) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return map_.erase(key) == 1U;
}

bool UnorderedMapKeyValueStoreService::clear() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  map_.clear();
  return true;
}

void UnorderedMapKeyValueStoreService::initialize() {
  ControllerService::initialize();
  setSupportedProperties(properties());
}

bool UnorderedMapKeyValueStoreService::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool exists = false;
  std::string value;
  auto it = map_.find(key);
  if (it != map_.end()) {
    exists = true;
    value = it->second;
  }
  try {
    if (!update_func(exists, value)) {
      return false;
    }
  } catch (const std::exception& e) {
    logger_->log_error("update_func failed with an exception: %s", e.what());
    return false;
  } catch (...) {
    logger_->log_error("update_func failed with an exception");
    return false;
  }
  if (!exists) {
    it = map_.emplace(key, "").first;
  }
  it->second = std::move(value);
  return true;
}

REGISTER_RESOURCE(UnorderedMapKeyValueStoreService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
