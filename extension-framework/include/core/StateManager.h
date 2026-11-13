// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once


#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/Core.h"
#include "minifi-cpp/core/StateManager.h"

namespace org::apache::nifi::minifi::core {

/**
 * Stores state for one component.
 * Supported operations: get(), set(), clear(), persist().
 * Behavior can be transactional. Use beginTransaction() to enter a transaction and commit() or rollback() to conclude it.
 */
class StateManagerImpl : public virtual StateManager {
 public:
  explicit StateManagerImpl(const utils::Identifier& id)
    : id_(id) {
  }

  ~StateManagerImpl() override = default;

  using StateManager::get;
  std::optional<State> get() override {
    if (State out; get(out)) {
      return out;
    }
    return std::nullopt;
  }

 protected:
  utils::Identifier id_;
};

}  // namespace org::apache::nifi::minifi::core
