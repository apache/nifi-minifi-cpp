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
#include <memory>

#include "controllers/keyvalue/AutoPersistor.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "../database/RocksDatabase.h"

#include "rocksdb/options.h"

namespace org::apache::nifi::minifi::controllers {

class RocksDbStateStorage : public KeyValueStateStorage {
 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.state.storage.local.encryption.key";
  static constexpr const char* ENCRYPTION_KEY_NAME_OLD = "nifi.state.management.provider.local.encryption.key";

  explicit RocksDbStateStorage(const std::string& name, const utils::Identifier& uuid = {});

  ~RocksDbStateStorage() override;

  EXTENSIONAPI static constexpr const char* Description = "A state storage service implemented by RocksDB";

  EXTENSIONAPI static const core::Property AlwaysPersist;
  EXTENSIONAPI static const core::Property AutoPersistenceInterval;
  EXTENSIONAPI static const core::Property Directory;
  static auto properties() {
    return std::array{
      AlwaysPersist,
      AutoPersistenceInterval,
      Directory
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;
  void onEnable() override;
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
  // non-virtual to allow calling on AutoPersistor's thread during destruction
  bool persistNonVirtual();

  std::string directory_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  rocksdb::WriteOptions default_write_options;
  AutoPersistor auto_persistor_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RocksDbStateStorage>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
