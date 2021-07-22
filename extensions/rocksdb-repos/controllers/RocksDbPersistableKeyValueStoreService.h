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

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "../database/RocksDatabase.h"

#include "rocksdb/options.h"



namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class RocksDbPersistableKeyValueStoreService : public AbstractAutoPersistingKeyValueStoreService {
 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.state.management.provider.local.encryption.key";

  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, const utils::Identifier& uuid = {});

  ~RocksDbPersistableKeyValueStoreService() override = default;

  static core::Property Directory;

  void initialize() override;
  void onEnable() override;
  void notifyStop() override;

  bool set(const std::string& key, const std::string& value) override;

  bool get(const std::string& key, std::string& value) override;

  bool get(std::unordered_map<std::string, std::string>& kvs) override;

  bool remove(const std::string& key) override;

  bool clear() override;

  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  bool persist() override;

 protected:
  std::string directory_;

  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  rocksdb::WriteOptions default_write_options;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
