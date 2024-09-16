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

#include <memory>
#include <string>
#include <unordered_map>

#include "core/StateManager.h"
#include "core/Core.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::controllers {

class KeyValueStateStorage;

class KeyValueStateManager final : public core::StateManagerImpl {
 public:
  KeyValueStateManager(const utils::Identifier& id, gsl::not_null<KeyValueStateStorage*> storage);

  bool set(const core::StateManager::State& kvs) override;
  bool get(core::StateManager::State& kvs) override;
  bool clear() override;
  bool persist() override;

  [[nodiscard]] bool isTransactionInProgress() const override;
  bool beginTransaction() override;
  bool commit() override;
  bool rollback() override;

 private:
  enum class ChangeType {
    NONE,
    SET,
    CLEAR
  };

  gsl::not_null<KeyValueStateStorage*> storage_;
  std::optional<core::StateManager::State> state_;
  bool transaction_in_progress_;
  ChangeType change_type_;
  core::StateManager::State state_to_set_;
};

}  // namespace org::apache::nifi::minifi::controllers
