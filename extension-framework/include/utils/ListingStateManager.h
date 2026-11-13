// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <utility>

#include "minifi-cpp/core/StateManager.h"
#include "minifi-cpp/core/logging/Logger.h"
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
  explicit ListingStateManager(core::StateManager& state_manager)
    : state_manager_(&state_manager) {
  }

  [[nodiscard]] ListingState getCurrentState() const;
  void storeState(const ListingState &latest_listing_state);

 private:
  [[nodiscard]] uint64_t getLatestListedKeyTimestampInMilliseconds(const std::unordered_map<std::string, std::string> &state) const;
  [[nodiscard]] static std::unordered_set<std::string> getLatestListedKeys(const std::unordered_map<std::string, std::string> &state);

  gsl::not_null<core::StateManager*> state_manager_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ListingStateManager>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::utils
