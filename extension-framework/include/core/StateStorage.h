// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "core/Core.h"
#include "minifi-cpp/core/StateManager.h"
#include "minifi-cpp/core/StateStorage.h"

namespace org::apache::nifi::minifi::core {

/**
 * Serves as a state storage background for the entire application, all StateManagers are created by it and use it.
 */
class StateStorageImpl : public virtual StateStorage {
 public:
  ~StateStorageImpl() override = default;

  using StateStorage::createStateManager;
  std::unique_ptr<StateManager> createStateManager(const CoreComponent& component) override {
    return createStateManager(component.getUUID());
  }
};

}  // namespace org::apache::nifi::minifi::core
