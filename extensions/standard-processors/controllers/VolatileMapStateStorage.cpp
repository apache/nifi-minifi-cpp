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

#include "VolatileMapStateStorage.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

VolatileMapStateStorage::VolatileMapStateStorage(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : KeyValueStateStorage(name, uuid) {
}

VolatileMapStateStorage::VolatileMapStateStorage(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : KeyValueStateStorage(name) {
  setConfiguration(configuration);
}

void VolatileMapStateStorage::initialize() {
  ControllerServiceImpl::initialize();
  setSupportedProperties(Properties);
}

bool VolatileMapStateStorage::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.set(key, value);
}

bool VolatileMapStateStorage::get(const std::string& key, std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(key, value);
}

bool VolatileMapStateStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(kvs);
}

bool VolatileMapStateStorage::remove(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.remove(key);
}

bool VolatileMapStateStorage::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.clear();
}

bool VolatileMapStateStorage::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.update(key, update_func);
}

REGISTER_RESOURCE_AS(VolatileMapStateStorage, ControllerService, ("UnorderedMapKeyValueStoreService", "VolatileMapStateStorage"));

}  // namespace org::apache::nifi::minifi::controllers
