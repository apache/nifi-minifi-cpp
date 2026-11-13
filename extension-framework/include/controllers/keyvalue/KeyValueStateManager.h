// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "core/StateManager.h"
#include "core/Core.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::controllers {

class KeyValueStateStorage;

class KeyValueStateManager final : public core::StateManagerImpl {
 public:
  KeyValueStateManager(const utils::Identifier& id, gsl::not_null<KeyValueStateStorage*> storage);

  using StateManagerImpl::get;
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
