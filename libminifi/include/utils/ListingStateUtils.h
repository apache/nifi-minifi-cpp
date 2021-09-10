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

#include <vector>
#include <string>
#include <unordered_map>

#include "core/CoreComponentState.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::utils {

class ListedObject {
 public:
  virtual uint64_t getLastModified() const = 0;
  virtual std::string getKey() const = 0;
};

struct ListingState {
  uint64_t listed_key_timestamp = 0;
  std::vector<std::string> listed_keys;

  bool wasObjectListedAlready(const ListedObject &object_attributes) const;
  void updateState(const ListedObject &object_attributes);
};

class ListingStateManager {
 public:
  explicit ListingStateManager(const std::shared_ptr<core::CoreComponentStateManager>& state_manager)
    : state_manager_(state_manager) {
  }

  ListingState getCurrentState() const;
  void storeState(const ListingState &latest_listing_state);

 private:
  static const std::string LATEST_LISTED_OBJECT_PREFIX;
  static const std::string LATEST_LISTED_OBJECT_TIMESTAMP;

  uint64_t getLatestListedKeyTimestamp(const std::unordered_map<std::string, std::string> &state) const;
  std::vector<std::string> getLatestListedKeys(const std::unordered_map<std::string, std::string> &state) const;

  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  const std::string timestamp_key_;
  const std::string listed_object_prefix_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ListingState>::getLogger()};
};

}  // namsspace org::apache::nifi::minifi::utils
