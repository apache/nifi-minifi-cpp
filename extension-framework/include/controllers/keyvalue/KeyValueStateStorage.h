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

#include "core/controller/ControllerServiceBase.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/StateManager.h"
#include "core/StateStorage.h"
#include "minifi-cpp/controllers/keyvalue/KeyValueStateStorage.h"

namespace org::apache::nifi::minifi::controllers {

class KeyValueStateStorage : public core::StateStorageImpl, public core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  static core::StateManager::State deserialize(const std::string& serialized);
  static std::string serialize(const core::StateManager::State& kvs);

  using core::StateStorageImpl::getStateManager;
  std::unique_ptr<core::StateManager> getStateManager(const utils::Identifier& uuid) override;
  std::unordered_map<utils::Identifier, core::StateManager::State> getAllStates() override;

  virtual bool set(const std::string& key, const std::string& value) = 0;
  virtual bool get(const std::string& key, std::string& value) = 0;
  virtual bool get(std::unordered_map<std::string, std::string>& kvs) = 0;
  virtual bool remove(const std::string& key) = 0;
  virtual bool clear() = 0;
  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) = 0;
  virtual bool persist() = 0;
  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

 private:
  bool getAll(std::unordered_map<utils::Identifier, std::string>& kvs);
};

}  // namespace org::apache::nifi::minifi::controllers
