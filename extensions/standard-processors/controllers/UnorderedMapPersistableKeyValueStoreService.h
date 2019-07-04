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
#ifndef __UNORDERED_MAP_PERSISTABLE_KEY_VALUE_STORE_SERVICE_H__
#define __UNORDERED_MAP_PERSISTABLE_KEY_VALUE_STORE_SERVICE_H__

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include "UnorderedMapKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

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

class UnorderedMapPersistableKeyValueStoreService : public AbstractAutoPersistingKeyValueStoreService,
                                                    public UnorderedMapKeyValueStoreService {
 public:
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::string& id);
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  virtual ~UnorderedMapPersistableKeyValueStoreService();

  static core::Property File;

  virtual void onEnable() override;
  virtual void initialize() override;
  virtual void notifyStop() override;

  virtual bool set(const std::string& key, const std::string& value) override;

  virtual bool remove(const std::string& key) override;

  virtual bool clear() override;

  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  virtual bool persist() override;

 protected:
  static constexpr const char* FORMAT_VERSION_KEY = "__UnorderedMapPersistableKeyValueStoreService_FormatVersion";
  static constexpr int FORMAT_VERSION = 1;

  std::string file_;

  bool load();

  std::string escape(const std::string& str);
  bool parseLine(const std::string& line, std::string& key, std::string& value);

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(UnorderedMapPersistableKeyValueStoreService, "A persistable key-value service implemented by a locked std::unordered_map<std::string, std::string> and persisted into a file");

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* __UNORDERED_MAP_PERSISTABLE_KEY_VALUE_STORE_SERVICE_H__ */
