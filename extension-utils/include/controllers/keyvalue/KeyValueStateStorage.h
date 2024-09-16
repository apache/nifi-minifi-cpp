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

#include "core/controller/ControllerService.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/StateManager.h"
#include "core/StateStorage.h"

namespace org::apache::nifi::minifi::controllers {

constexpr const char* ALWAYS_PERSIST_PROPERTY_NAME = "Always Persist";
constexpr const char* AUTO_PERSISTENCE_INTERVAL_PROPERTY_NAME = "Auto Persistence Interval";

class KeyValueStateStorage : public core::StateStorageImpl, public core::controller::ControllerServiceImpl {
 public:
  explicit KeyValueStateStorage(const std::string& name, const utils::Identifier& uuid = {});

  static core::StateManager::State deserialize(const std::string& serialized);
  static std::string serialize(const core::StateManager::State& kvs);

  using core::StateStorageImpl::getStateManager;
  std::unique_ptr<core::StateManager> getStateManager(const utils::Identifier& uuid) override;
  std::unordered_map<utils::Identifier, core::StateManager::State> getAllStates() override;

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  virtual bool set(const std::string& key, const std::string& value) = 0;
  virtual bool get(const std::string& key, std::string& value) = 0;
  virtual bool get(std::unordered_map<std::string, std::string>& kvs) = 0;
  virtual bool remove(const std::string& key) = 0;
  virtual bool clear() = 0;
  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) = 0;
  virtual bool persist() = 0;

 private:
  bool getAll(std::unordered_map<utils::Identifier, std::string>& kvs);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<KeyValueStateStorage>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
