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

#include "controllers/keyvalue/KeyValueStateManager.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"

namespace org::apache::nifi::minifi::controllers {

KeyValueStateManager::KeyValueStateManager(
        const utils::Identifier& id,
        gsl::not_null<KeyValueStateStorage*> storage)
    : StateManagerImpl(id),
      storage_(storage),
      transaction_in_progress_(false),
      change_type_(ChangeType::NONE) {
  std::string serialized;
  if (storage_->get(id_.to_string(), serialized)) {
    state_ = KeyValueStateStorage::deserialize(serialized);
  }
}

bool KeyValueStateManager::set(const core::StateManager::State& kvs) {
  bool auto_commit = false;
  if (!transaction_in_progress_) {
    auto_commit = true;
    transaction_in_progress_ = true;
  }

  change_type_ = ChangeType::SET;
  state_to_set_ = kvs;

  if (auto_commit) {
    return commit();
  }
  return true;
}

bool KeyValueStateManager::get(core::StateManager::State& kvs) {
  if (!state_) {
    return false;
  }
  // not allowed, if there were modifications (dirty read)
  if (change_type_ != ChangeType::NONE) {
    return false;
  }
  kvs = *state_;
  return true;
}

bool KeyValueStateManager::clear() {
  if (!state_) {
    return false;
  }

  bool auto_commit = false;
  if (!transaction_in_progress_) {
    auto_commit = true;
    transaction_in_progress_ = true;
  }

  change_type_ = ChangeType::CLEAR;
  state_to_set_.clear();

  if (auto_commit) {
    return commit();
  }
  return true;
}

bool KeyValueStateManager::persist() {
  return storage_->persist();
}

bool KeyValueStateManager::isTransactionInProgress() const {
  return transaction_in_progress_;
}

bool KeyValueStateManager::beginTransaction() {
  if (transaction_in_progress_) {
    return false;
  }
  transaction_in_progress_ = true;
  return true;
}

bool KeyValueStateManager::commit() {
  if (!transaction_in_progress_) {
    return true;
  }

  bool success = true;

  // actually make the pending changes
  if (change_type_ == ChangeType::SET) {
    if (storage_->set(id_.to_string(), KeyValueStateStorage::serialize(state_to_set_))) {
      state_ = state_to_set_;
    } else {
      success = false;
    }
  } else if (change_type_ == ChangeType::CLEAR) {
    if (state_ && storage_->remove(id_.to_string())) {
      state_.reset();
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

bool KeyValueStateManager::rollback() {
  if (!transaction_in_progress_) {
    return true;
  }

  change_type_ = ChangeType::NONE;
  state_to_set_.clear();
  transaction_in_progress_ = false;
  return true;
}

}  // namespace org::apache::nifi::minifi::controllers
