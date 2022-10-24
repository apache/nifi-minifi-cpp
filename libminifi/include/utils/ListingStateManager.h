/**
 *
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

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <utility>

#include "core/StateManager.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils {

class ListedObject {
 public:
  [[nodiscard]] virtual std::chrono::time_point<std::chrono::system_clock> getLastModified() const = 0;
  [[nodiscard]] virtual std::string getKey() const = 0;
  virtual ~ListedObject() = default;
};

struct ListingState {
  [[nodiscard]] bool wasObjectListedAlready(const ListedObject &object_attributes) const;
  void updateState(const ListedObject &object_attributes);
  uint64_t getListedKeyTimeStampInMilliseconds() const;

  std::chrono::time_point<std::chrono::system_clock> listed_key_timestamp;
  std::unordered_set<std::string> listed_keys;
};

class ListingStateManager {
 public:
  explicit ListingStateManager(core::StateManager* state_manager)
    : state_manager_(state_manager) {
  }

  [[nodiscard]] ListingState getCurrentState() const;
  void storeState(const ListingState &latest_listing_state);

 private:
  static const std::string LATEST_LISTED_OBJECT_PREFIX;
  static const std::string LATEST_LISTED_OBJECT_TIMESTAMP;

  [[nodiscard]] static uint64_t getLatestListedKeyTimestampInMilliseconds(const std::unordered_map<std::string, std::string> &state);
  [[nodiscard]] static std::unordered_set<std::string> getLatestListedKeys(const std::unordered_map<std::string, std::string> &state);

  core::StateManager* state_manager_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ListingStateManager>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::utils
