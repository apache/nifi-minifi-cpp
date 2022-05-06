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
#include "Exception.h"

#include <memory>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

namespace {
using org::apache::nifi::minifi::core::CoreComponentState;

std::string serialize(const CoreComponentState &kvs) {
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

CoreComponentState deserialize(const std::string &serialized) {
  rapidjson::StringStream stream(serialized.c_str());
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.ParseStream(stream);
  if (!res || !doc.IsObject()) {
    using org::apache::nifi::minifi::Exception;
    using org::apache::nifi::minifi::FILE_OPERATION_EXCEPTION;
    throw Exception(FILE_OPERATION_EXCEPTION, "Could not deserialize saved state, error during JSON parsing.");
  }

  CoreComponentState retState;
  for (const auto &kv : doc.GetObject()) {
    retState[kv.name.GetString()] = kv.value.GetString();
  }

  return retState;
}
}  // anonymous namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::AbstractCoreComponentStateManager(
    AbstractCoreComponentStateManagerProvider* provider,
    const utils::Identifier& id)
    : provider_(provider)
    , id_(id)
    , state_valid_(false)
    , transaction_in_progress_(false)
    , change_type_(ChangeType::NONE) {
  std::string serialized;
  if (provider_->getImpl(id_, serialized)) {
    state_ = deserialize(serialized);
    state_valid_ = true;
  }
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::set(const core::CoreComponentState& kvs) {
  bool autoCommit = false;
  if (!transaction_in_progress_) {
    autoCommit = true;
    transaction_in_progress_ = true;
  }

  change_type_ = ChangeType::SET;
  state_to_set_ = kvs;

  if (autoCommit) {
    return commit();
  }
  return true;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::get(core::CoreComponentState& kvs) {
  if (!state_valid_) {
    return false;
  }
  // not allowed, if there were modifications (dirty read)
  if (change_type_ != ChangeType::NONE) {
    return false;
  }
  kvs = state_;
  return true;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::clear() {
  if (!state_valid_) {
    return false;
  }

  bool autoCommit = false;
  if (!transaction_in_progress_) {
    autoCommit = true;
    transaction_in_progress_ = true;
  }

  change_type_ = ChangeType::CLEAR;
  state_to_set_.clear();

  if (autoCommit) {
    return commit();
  }
  return true;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::persist() {
  return provider_->persistImpl();
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::isTransactionInProgress() const {
  return transaction_in_progress_;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::beginTransaction() {
  if (transaction_in_progress_) {
    return false;
  }
  transaction_in_progress_ = true;
  return true;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::commit() {
  if (!transaction_in_progress_) {
    return false;
  }

  bool success = true;

  // actually make the pending changes
  if (change_type_ == ChangeType::SET) {
    if (provider_->setImpl(id_, serialize(state_to_set_))) {
      state_valid_ = true;
      state_ = state_to_set_;
    } else {
      success = false;
    }
  } else if (change_type_ == ChangeType::CLEAR) {
    if (!state_valid_) { // NOLINT (bugprone-branch-clone)
      success = false;
    } else if (provider_->removeImpl(id_)) {
      state_valid_ = false;
      state_.clear();
    } else {
      success = false;
    }
  }

  if (success && change_type_ != ChangeType::NONE) {
    success = persist();
  }

  change_type_ = ChangeType::NONE;
  state_to_set_.clear();
  transaction_in_progress_ = false;
  return success;
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::rollback() {
  if (!transaction_in_progress_) {
    return false;
  }

  change_type_ = ChangeType::NONE;
  state_to_set_.clear();
  transaction_in_progress_ = false;
  return true;
}

std::unique_ptr<core::CoreComponentStateManager> AbstractCoreComponentStateManagerProvider::getCoreComponentStateManager(const utils::Identifier& uuid) {
  std::lock_guard<std::mutex> guard(mutex_);
  return std::make_unique<AbstractCoreComponentStateManager>(this, uuid);
}

std::map<utils::Identifier, core::CoreComponentState> AbstractCoreComponentStateManagerProvider::getAllCoreComponentStates() {
  std::map<utils::Identifier, std::string> all_serialized;
  if (!getImpl(all_serialized)) {
    return {};
  }

  std::map<utils::Identifier, core::CoreComponentState> all_deserialized;
  for (const auto& serialized : all_serialized) {
    all_deserialized.emplace(serialized.first, deserialize(serialized.second));
  }

  return all_deserialized;
}

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
