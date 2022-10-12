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

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include "UnorderedMapKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::controllers {

class UnorderedMapPersistableKeyValueStoreService : public AbstractAutoPersistingKeyValueStoreService,
                                                    public UnorderedMapKeyValueStoreService {
 public:
  explicit UnorderedMapPersistableKeyValueStoreService(std::string name, const utils::Identifier& uuid = {});
  explicit UnorderedMapPersistableKeyValueStoreService(std::string name, const std::shared_ptr<Configure>& configuration);

  ~UnorderedMapPersistableKeyValueStoreService() override;

  EXTENSIONAPI static constexpr const char* Description = "A persistable key-value service implemented by a locked std::unordered_map<std::string, std::string> and persisted into a file";
  EXTENSIONAPI static const core::Property LinkedServices;
  EXTENSIONAPI static const core::Property AlwaysPersist;
  EXTENSIONAPI static const core::Property AutoPersistenceInterval;
  EXTENSIONAPI static const core::Property Directory;
  EXTENSIONAPI static const core::Property File;
  static auto properties() {
    return std::array{
      LinkedServices,
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

  bool remove(const std::string& key) override;

  bool clear() override;

  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  bool persist() override {
    return persistNonVirtual();
  }

 protected:
  using AbstractAutoPersistingKeyValueStoreService::getImpl;
  using AbstractAutoPersistingKeyValueStoreService::setImpl;
  using AbstractAutoPersistingKeyValueStoreService::persistImpl;
  using AbstractAutoPersistingKeyValueStoreService::removeImpl;


  static constexpr const char* FORMAT_VERSION_KEY = "__UnorderedMapPersistableKeyValueStoreService_FormatVersion";
  static constexpr int FORMAT_VERSION = 1;

  std::string file_;

  bool load();

  bool parseLine(const std::string& line, std::string& key, std::string& value);

 private:
  bool persistNonVirtual();

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger();
};

static_assert(std::is_convertible<UnorderedMapKeyValueStoreService*, PersistableKeyValueStoreService*>::value, "UnorderedMapKeyValueStoreService is a PersistableKeyValueStoreService");

}  // namespace org::apache::nifi::minifi::controllers
