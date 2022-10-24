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

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <utility>

#include "controllers/keyvalue/AutoPersistor.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "InMemoryKeyValueStorage.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::controllers {

class PersistentMapStateStorage : public KeyValueStateStorage {
 public:
  explicit PersistentMapStateStorage(const std::string& name, const utils::Identifier& uuid = {});
  explicit PersistentMapStateStorage(const std::string& name, const std::shared_ptr<Configure>& configuration);

  ~PersistentMapStateStorage() override;

  EXTENSIONAPI static constexpr const char* Description = "A persistable state storage service implemented by a locked std::unordered_map<std::string, std::string> and persisted into a file";

  EXTENSIONAPI static const core::Property AlwaysPersist;
  EXTENSIONAPI static const core::Property AutoPersistenceInterval;
  EXTENSIONAPI static const core::Property File;
  static auto properties() {
    return std::array{
      AlwaysPersist,
      AutoPersistenceInterval,
      File
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void onEnable() override;
  void initialize() override;
  void notifyStop() override;

  bool set(const std::string& key, const std::string& value) override;
  bool get(const std::string& key, std::string& value) override;
  bool get(std::unordered_map<std::string, std::string>& kvs) override;
  bool remove(const std::string& key) override;
  bool clear() override;
  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  bool persist() override {
    return persistNonVirtual();
  }

 private:
  static constexpr const char* FORMAT_VERSION_KEY = "__UnorderedMapPersistableKeyValueStoreService_FormatVersion";
  static constexpr int FORMAT_VERSION = 1;

  bool load();
  bool parseLine(const std::string& line, std::string& key, std::string& value);

  // non-virtual to allow calling in destructor
  bool persistNonVirtual();

  std::mutex mutex_;
  std::string file_;
  InMemoryKeyValueStorage storage_;
  AutoPersistor auto_persistor_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PersistentMapStateStorage>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
