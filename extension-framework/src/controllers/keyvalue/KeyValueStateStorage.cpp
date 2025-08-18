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

#include <memory>

#include "Exception.h"
#include "controllers/keyvalue/KeyValueStateManager.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/gsl.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

namespace org::apache::nifi::minifi::controllers {

std::string KeyValueStateStorage::serialize(const core::StateManager::State& kvs) {
  rapidjson::Document doc(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType &alloc = doc.GetAllocator();
  for (const auto &kv : kvs) {
    doc.AddMember(rapidjson::StringRef(kv.first.c_str(), kv.first.size()),
                  rapidjson::StringRef(kv.second.c_str(), kv.second.size()), alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  return buffer.GetString();
}

core::StateManager::State KeyValueStateStorage::deserialize(const std::string& serialized) {
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(serialized.c_str(), serialized.length());
  if (!res || !doc.IsObject()) {
    using org::apache::nifi::minifi::Exception;
    using org::apache::nifi::minifi::FILE_OPERATION_EXCEPTION;
    throw Exception(FILE_OPERATION_EXCEPTION, "Could not deserialize saved state, error during JSON parsing.");
  }

  core::StateManager::State retState;
  for (const auto &kv : doc.GetObject()) {
    retState[kv.name.GetString()] = kv.value.GetString();
  }

  return retState;
}

KeyValueStateStorage::KeyValueStateStorage(const std::string& name, const utils::Identifier& uuid)
  : ControllerServiceImpl(name, uuid) {
}

std::unique_ptr<core::StateManager> KeyValueStateStorage::getStateManager(const utils::Identifier& uuid) {
  return std::make_unique<KeyValueStateManager>(uuid, gsl::make_not_null(this));
}

std::unordered_map<utils::Identifier, core::StateManager::State> KeyValueStateStorage::getAllStates() {
  std::unordered_map<utils::Identifier, std::string> all_serialized;
  if (!getAll(all_serialized)) {
    return {};
  }

  std::unordered_map<utils::Identifier, core::StateManager::State> all_deserialized;
  for (const auto& serialized : all_serialized) {
    all_deserialized.emplace(serialized.first, deserialize(serialized.second));
  }

  return all_deserialized;
}

bool KeyValueStateStorage::getAll(std::unordered_map<utils::Identifier, std::string>& kvs) {
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
      logger_->log_error("Found non-UUID key \"{}\" in storage implementation", state.first);
    }
  }
  return true;
}

}  // namespace org::apache::nifi::minifi::controllers
