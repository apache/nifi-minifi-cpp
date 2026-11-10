// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "Core.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace org::apache::nifi::minifi::core {

/**
 * Stores state for one component.
 * Supported operations: get(), set(), clear(), persist().
 * Behavior can be transactional. Use beginTransaction() to enter a transaction and commit() or rollback() to conclude it.
 */
class StateManager {
 public:
  using State = std::unordered_map<std::string, std::string>;

  virtual ~StateManager() = default;
  virtual bool set(const State& kvs) = 0;
  virtual bool get(State& kvs) = 0;
  virtual std::optional<State> get() = 0;

  virtual bool clear() = 0;
  virtual bool persist() = 0;

  [[nodiscard]] virtual bool isTransactionInProgress() const = 0;

  virtual bool beginTransaction() = 0;
  virtual bool commit() = 0;
  virtual bool rollback() = 0;
};

}  // namespace org::apache::nifi::minifi::core
