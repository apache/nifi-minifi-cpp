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
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
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

  EXTENSIONAPI static constexpr auto AlwaysPersist = core::PropertyDefinitionBuilder<>::createProperty(ALWAYS_PERSIST_PROPERTY_NAME)
      .withDescription("Persist every change instead of persisting it periodically.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto AutoPersistenceInterval = core::PropertyDefinitionBuilder<>::createProperty(AUTO_PERSISTENCE_INTERVAL_PROPERTY_NAME)
      .withDescription("The interval of the periodic task persisting all values. Only used if Always Persist is false. If set to 0 seconds, auto persistence will be disabled.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("1 min")
      .build();
  EXTENSIONAPI static constexpr auto Directory = core::PropertyDefinitionBuilder<>::createProperty("Directory")
      .withDescription("Path to a directory for the database")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      AlwaysPersist,
      AutoPersistenceInterval,
      Directory
  });


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
