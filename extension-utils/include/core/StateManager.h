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

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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
