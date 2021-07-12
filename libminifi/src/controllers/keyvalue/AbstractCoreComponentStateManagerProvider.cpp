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

#include "controllers/keyvalue/AbstractCoreComponentStateManagerProvider.h"

#include <memory>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#ifdef GetObject
#undef GetObject
#endif /* GetObject -> GetObject[AW] on windows */

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::AbstractCoreComponentStateManager(
    std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider,
    const utils::Identifier& id)
    : provider_(std::move(provider))
    , id_(id)
    , state_valid_(false) {
  std::string serialized;
  if (provider_->getImpl(id_, serialized) && provider_->deserialize(serialized, state_)) {
    state_valid_ = true;
  }
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::set(const core::CoreComponentState& kvs) {
  if (provider_->setImpl(id_, provider_->serialize(kvs))) {
    state_valid_ = true;
    state_ = kvs;
    return true;
  } else {
    return false;
  }
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::get(core::CoreComponentState& kvs) {
  if (!state_valid_) {
    return false;
  }
  kvs = state_;
  return true;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::clear() {
  if (!state_valid_) {
    return false;
  }
  if (provider_->removeImpl(id_)) {
    state_valid_ = false;
    state_.clear();
    return true;
  } else {
    return false;
  }
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::persist() {
  if (!state_valid_) {
    return false;
  }
  return provider_->persistImpl();
}

AbstractCoreComponentStateManagerProvider::~AbstractCoreComponentStateManagerProvider() = default;

std::shared_ptr<core::CoreComponentStateManager> AbstractCoreComponentStateManagerProvider::getCoreComponentStateManager(const utils::Identifier& uuid) {
  return std::make_shared<AbstractCoreComponentStateManager>(shared_from_this(), uuid);
}

std::map<utils::Identifier, core::CoreComponentState> AbstractCoreComponentStateManagerProvider::getAllCoreComponentStates() {
  std::map<utils::Identifier, std::string> all_serialized;
  if (!getImpl(all_serialized)) {
    return {};
  }

  std::map<utils::Identifier, core::CoreComponentState> all_deserialized;
  for (const auto& serialized : all_serialized) {
    core::CoreComponentState deserialized;
    if (deserialize(serialized.second, deserialized)) {
      all_deserialized.emplace(serialized.first, std::move(deserialized));
    }
  }

  return all_deserialized;
}

std::string AbstractCoreComponentStateManagerProvider::serialize(const core::CoreComponentState& kvs) {
  rapidjson::Document doc(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType &alloc = doc.GetAllocator();
  for (const auto& kv : kvs) {
    doc.AddMember(rapidjson::StringRef(kv.first.c_str(), kv.first.size()), rapidjson::StringRef(kv.second.c_str(), kv.second.size()), alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  return buffer.GetString();
}

bool AbstractCoreComponentStateManagerProvider::deserialize(const std::string& serialized, core::CoreComponentState& kvs) {
  rapidjson::StringStream stream(serialized.c_str());
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.ParseStream(stream);
  if (!res) {
    return false;
  }
  if (!doc.IsObject()) {
    return false;
  }

  kvs.clear();
  for (const auto& kv : doc.GetObject()) {
    kvs[kv.name.GetString()] = kv.value.GetString();
  }

  return true;
}

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
