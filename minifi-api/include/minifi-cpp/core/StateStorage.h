// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "Core.h"
#include "StateManager.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace org::apache::nifi::minifi::core {

/**
 * Serves as a state storage background for the entire application, all StateManagers are created by it and use it.
 */
class StateStorage {
 public:
  virtual ~StateStorage() = default;

  virtual std::unique_ptr<StateManager> createStateManager(const utils::Identifier& uuid) = 0;

  virtual std::unique_ptr<StateManager> createStateManager(const CoreComponent& component) = 0;

  virtual std::unordered_map<utils::Identifier, StateManager::State> getAllStates() = 0;
};

}  // namespace org::apache::nifi::minifi::core
