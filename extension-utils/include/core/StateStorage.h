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

#include "core/Core.h"
#include "StateManager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include "minifi-cpp/core/StateStorage.h"

namespace org::apache::nifi::minifi::core {

/**
 * Serves as a state storage background for the entire application, all StateManagers are created by it and use it.
 */
class StateStorageImpl : public virtual StateStorage {
 public:
  ~StateStorageImpl() override = default;

  using StateStorage::getStateManager;
  std::unique_ptr<StateManager> getStateManager(const CoreComponent& component) override {
    return getStateManager(component.getUUID());
  }
};

}  // namespace org::apache::nifi::minifi::core
