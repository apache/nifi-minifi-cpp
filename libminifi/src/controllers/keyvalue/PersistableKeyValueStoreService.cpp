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

#include "controllers/keyvalue/PersistableKeyValueStoreService.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::controllers {

PersistableKeyValueStoreService::PersistableKeyValueStoreService(std::string name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(std::move(name), uuid) {
}

PersistableKeyValueStoreService::~PersistableKeyValueStoreService() = default;

bool PersistableKeyValueStoreService::setImpl(const utils::Identifier& key, const std::string& serialized_state) {
  return set(key.to_string(), serialized_state);
}

bool PersistableKeyValueStoreService::getImpl(const utils::Identifier& key, std::string& serialized_state) {
  return get(key.to_string(), serialized_state);
}

bool PersistableKeyValueStoreService::getImpl(std::map<utils::Identifier, std::string>& kvs) {
  std::unordered_map<std::string, std::string> states;
  if (!get(states)) {
    return false;
  }
  kvs.clear();
  for (const auto& state : states) {
    const auto optional_uuid = utils::Identifier::parse(state.first);
    if (optional_uuid) {
      kvs[optional_uuid.value()] = state.second;
    } else {
      core::logging::LoggerFactory<PersistableKeyValueStoreService>::getLogger()
          ->log_error("Found non-UUID key \"%s\" in storage implementation", state.first);
    }
  }
  return true;
}

bool PersistableKeyValueStoreService::removeImpl(const utils::Identifier& key) {
  return remove(key.to_string());
}

bool PersistableKeyValueStoreService::persistImpl() {
  return persist();
}

}  // namespace org::apache::nifi::minifi::controllers
