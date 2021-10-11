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

#include "utils/ListingStateUtils.h"

#include <algorithm>

#include "core/Property.h"

namespace org::apache::nifi::minifi::utils {

const std::string ListingStateManager::LATEST_LISTED_OBJECT_PREFIX = "listed_key.";
const std::string ListingStateManager::LATEST_LISTED_OBJECT_TIMESTAMP = "listed_timestamp";

bool ListingState::wasObjectListedAlready(const ListedObject &object) const {
  return listed_key_timestamp > object.getLastModified() ||
      (listed_key_timestamp == object.getLastModified() &&
        std::find(listed_keys.begin(), listed_keys.end(), object.getKey()) != listed_keys.end());
}

void ListingState::updateState(const ListedObject &object) {
  if (listed_key_timestamp < object.getLastModified()) {
    listed_key_timestamp = object.getLastModified();
    listed_keys.clear();
    listed_keys.push_back(object.getKey());
  } else if (listed_key_timestamp == object.getLastModified()) {
    listed_keys.push_back(object.getKey());
  }
}

uint64_t ListingStateManager::getLatestListedKeyTimestamp(const std::unordered_map<std::string, std::string> &state) const {
  std::string stored_listed_key_timestamp_str;
  auto it = state.find(LATEST_LISTED_OBJECT_TIMESTAMP);
  if (it != state.end()) {
    stored_listed_key_timestamp_str = it->second;
  }

  int64_t stored_listed_key_timestamp = 0;
  core::Property::StringToInt(stored_listed_key_timestamp_str, stored_listed_key_timestamp);

  return stored_listed_key_timestamp;
}

std::vector<std::string> ListingStateManager::getLatestListedKeys(const std::unordered_map<std::string, std::string> &state) const {
  std::vector<std::string> latest_listed_keys;
  for (const auto& kvp : state) {
    if (kvp.first.rfind(LATEST_LISTED_OBJECT_PREFIX, 0) == 0) {
      latest_listed_keys.push_back(kvp.second);
    }
  }
  return latest_listed_keys;
}

ListingState ListingStateManager::getCurrentState() const {
  ListingState current_listing_state;
  std::unordered_map<std::string, std::string> state;
  if (!state_manager_->get(state)) {
    logger_->log_info("No stored state for listed objects was found");
    return current_listing_state;
  }

  current_listing_state.listed_key_timestamp = getLatestListedKeyTimestamp(state);
  logger_->log_debug("Restored previous listed timestamp %lld", current_listing_state.listed_key_timestamp);

  current_listing_state.listed_keys = getLatestListedKeys(state);
  return current_listing_state;
}

void ListingStateManager::storeState(const ListingState &latest_listing_state) {
  std::unordered_map<std::string, std::string> state;
  state[LATEST_LISTED_OBJECT_TIMESTAMP] = std::to_string(latest_listing_state.listed_key_timestamp);
  for (std::size_t i = 0; i < latest_listing_state.listed_keys.size(); ++i) {
    state[LATEST_LISTED_OBJECT_PREFIX + std::to_string(i)] = latest_listing_state.listed_keys.at(i);
  }
  logger_->log_debug("Stored new listed timestamp %lld", latest_listing_state.listed_key_timestamp);
  state_manager_->set(state);
}

}  // namespace org::apache::nifi::minifi::utils
