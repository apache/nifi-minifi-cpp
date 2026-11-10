// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <unordered_map>
#include <string>
#include <memory>

#include "utils/AutoPersistor.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "core/Core.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "../database/RocksDatabase.h"

#include "rocksdb/options.h"

namespace org::apache::nifi::minifi::controllers {

class RocksDbStateStorage : public KeyValueStateStorage {
 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.state.storage.local.encryption.key";
  static constexpr const char* ENCRYPTION_KEY_NAME_OLD = "nifi.state.management.provider.local.encryption.key";

  using KeyValueStateStorage::KeyValueStateStorage;

  ~RocksDbStateStorage() override;

  EXTENSIONAPI static constexpr const char* Description = "A state storage service implemented by RocksDB";

  EXTENSIONAPI static constexpr auto AlwaysPersist = core::PropertyDefinitionBuilder<>::createProperty(ALWAYS_PERSIST_PROPERTY_NAME)
      .withDescription("Persist every change instead of persisting it periodically.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto AutoPersistenceInterval = core::PropertyDefinitionBuilder<>::createProperty(AUTO_PERSISTENCE_INTERVAL_PROPERTY_NAME)
      .withDescription("The interval of the periodic task persisting all values. Only used if Always Persist is false. If set to 0 seconds, auto persistence will be disabled.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
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
  bool verify_checksums_in_rocksdb_reads_ = false;
};

}  // namespace org::apache::nifi::minifi::controllers
