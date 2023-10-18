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

#include "InMemoryKeyValueStorage.h"

namespace org::apache::nifi::minifi::controllers {

bool InMemoryKeyValueStorage::set(const std::string& key, const std::string& value) {
  map_[key] = value;
  return true;
}

bool InMemoryKeyValueStorage::get(const std::string& key, std::string& value) {
  auto it = map_.find(key);
  if (it == map_.end()) {
    return false;
  } else {
    value = it->second;
    return true;
  }
}

bool InMemoryKeyValueStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  kvs = map_;
  return true;
}

bool InMemoryKeyValueStorage::remove(const std::string& key) {
  return map_.erase(key) == 1U;
}

bool InMemoryKeyValueStorage::clear() {
  map_.clear();
  return true;
}

bool InMemoryKeyValueStorage::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
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
    logger_->log_error("update_func failed with an exception: {}", e.what());
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

}  // namespace org::apache::nifi::minifi::controllers
