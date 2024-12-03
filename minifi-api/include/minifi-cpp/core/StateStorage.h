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

  virtual std::unique_ptr<StateManager> getStateManager(const utils::Identifier& uuid) = 0;

  virtual std::unique_ptr<StateManager> getStateManager(const CoreComponent& component) = 0;

  virtual std::unordered_map<utils::Identifier, StateManager::State> getAllStates() = 0;
};

}  // namespace org::apache::nifi::minifi::core
