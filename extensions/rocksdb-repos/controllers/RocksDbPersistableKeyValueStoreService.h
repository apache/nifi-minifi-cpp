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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_RocksDbPersistableKeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_RocksDbPersistableKeyValueStoreService_H_

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class RocksDbPersistableKeyValueStoreService : public AbstractAutoPersistingKeyValueStoreService {
 public:
  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, const std::string& id);
  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());

  virtual ~RocksDbPersistableKeyValueStoreService();

  static core::Property Directory;

  virtual void initialize() override;
  virtual void onEnable() override;
  virtual void notifyStop() override;

  virtual bool set(const std::string& key, const std::string& value) override;

  virtual bool get(const std::string& key, std::string& value) override;

  virtual bool get(std::unordered_map<std::string, std::string>& kvs) override;

  virtual bool remove(const std::string& key) override;

  virtual bool clear() override;

  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  virtual bool persist() override;

 protected:
  std::string directory_;

  rocksdb::DB* db_;
  rocksdb::WriteOptions default_write_options;
  bool db_valid_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_RocksDbPersistableKeyValueStoreService_H_ */
