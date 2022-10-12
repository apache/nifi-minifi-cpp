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
#pragma  once

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <utility>

#include "controllers/keyvalue/KeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "controllers/keyvalue/PersistableKeyValueStoreService.h"

namespace org::apache::nifi::minifi::controllers {

/// Key-value store service purely in RAM without disk usage
class UnorderedMapKeyValueStoreService : virtual public PersistableKeyValueStoreService {
 public:
  explicit UnorderedMapKeyValueStoreService(std::string name, const utils::Identifier& uuid = {});
  explicit UnorderedMapKeyValueStoreService(std::string name, const std::shared_ptr<Configure>& configuration);

  ~UnorderedMapKeyValueStoreService() override;

  EXTENSIONAPI static constexpr const char* Description = "A key-value service implemented by a locked std::unordered_map<std::string, std::string>";
  EXTENSIONAPI static const core::Property LinkedServices;
  static auto properties() { return std::array{LinkedServices}; }
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  bool set(const std::string& key, const std::string& value) override;
  bool get(const std::string& key, std::string& value) override;
  bool get(std::unordered_map<std::string, std::string>& kvs) override;
  bool remove(const std::string& key) override;
  bool clear() override;
  void initialize() override;
  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;
  bool persist() override {
    return true;
  }

 protected:
  std::unordered_map<std::string, std::string> map_;
  std::recursive_mutex mutex_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<UnorderedMapKeyValueStoreService>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
