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

#include "core/Core.h"
#include "minifi-cpp/properties/Configure.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "InMemoryKeyValueStorage.h"

namespace org::apache::nifi::minifi::controllers {

/// Key-value state storage purely in RAM without disk usage
class VolatileMapStateStorage : virtual public KeyValueStateStorage {
 public:
  using KeyValueStateStorage::KeyValueStateStorage;

  EXTENSIONAPI static constexpr const char* Description = "A key-value service implemented by a locked std::unordered_map<std::string, std::string>";
  EXTENSIONAPI static constexpr auto LinkedServices = core::PropertyDefinitionBuilder<>::createProperty("Linked Services")
      .withDescription("Referenced Controller Services")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({LinkedServices});
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;

  void initialize() override;

  bool set(const std::string& key, const std::string& value) override;
  bool get(const std::string& key, std::string& value) override;
  bool get(std::unordered_map<std::string, std::string>& kvs) override;
  bool remove(const std::string& key) override;
  bool clear() override;
  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  bool persist() override {
    return true;
  }

 private:
  std::mutex mutex_;
  InMemoryKeyValueStorage storage_;
};

}  // namespace org::apache::nifi::minifi::controllers
