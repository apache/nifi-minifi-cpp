/**
 * @file Configure.h
 * Configure class declaration
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

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/logging/Logger.h"
#include "utils/ChecksumCalculator.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/properties/Properties.h"

namespace org::apache::nifi::minifi {

class PropertiesImpl : public virtual Properties {
  struct PropertyValue {
    std::string persisted_value;
    std::string active_value;
    bool need_to_persist_new_value{false};
  };

 public:
  explicit PropertiesImpl(std::string name = "");

  ~PropertiesImpl() override = default;

  const std::string& getName() const override {
    return name_;
  }

  // Clear the load config
  void clear() override {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_.clear();
  }
  void set(const std::string& key, const std::string& value) override {
    set(key, value, PropertyChangeLifetime::PERSISTENT);
  }
  // Set the config value
  void set(const std::string &key, const std::string &value, PropertyChangeLifetime lifetime) override {
    auto active_value = utils::string::replaceEnvironmentVariables(value);
    std::lock_guard<std::mutex> lock(mutex_);
    bool should_persist = lifetime == PropertyChangeLifetime::PERSISTENT;
    if (auto it = properties_.find(key); it != properties_.end()) {
      // update an existing property
      it->second.active_value = active_value;
      if (should_persist) {
        it->second.persisted_value = value;
        it->second.need_to_persist_new_value = true;
      }
    } else {
      // brand-new property
      properties_[key] = PropertyValue{value, active_value, should_persist};
    }

    if (should_persist) {
      dirty_ = true;
    }
  }
  // Check whether the config value existed
  bool has(const std::string& key) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.count(key) > 0;
  }
  /**
   * Returns the config value by placing it into the referenced param value
   * @param key key to look up
   * @param value value in which to place the map's stored property value
   * @returns true if found, false otherwise.
   */
  bool getString(const std::string &key, std::string &value) const override;

  /**
   * Returns the configuration value or an empty string.
   * @return value corresponding to key or empty value.
   */
  int getInt(const std::string &key, int default_value) const override;

  /**
   * Returns the config value.
   *
   * @param key key to look up
   * @returns the value if found, nullopt otherwise.
   */
  std::optional<std::string> getString(const std::string& key) const override;

  /**
   * Load configure file
   * @param fileName path of the configuration file RELATIVE to MINIFI_HOME set by setHome()
   */
  void loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix = "") override;

  // Set the determined MINIFI_HOME
  void setHome(std::filesystem::path minifiHome) override {
    minifi_home_ = std::move(minifiHome);
  }

  std::vector<std::string> getConfiguredKeys() const override {
    std::vector<std::string> keys;
    for (auto &property : properties_) {
      keys.push_back(property.first);
    }
    return keys;
  }

  // Get the determined MINIFI_HOME
  std::filesystem::path getHome() const override {
    return minifi_home_;
  }

  bool commitChanges() override;

  utils::ChecksumCalculator& getChecksumCalculator() override { return checksum_calculator_; }

  std::filesystem::path getFilePath() const override;

  std::map<std::string, std::string> getProperties() const override;

 private:
  std::map<std::string, PropertyValue> properties_;

  bool dirty_{false};

  std::filesystem::path properties_file_;

  utils::ChecksumCalculator checksum_calculator_;

  // Mutex for protection
  mutable std::mutex mutex_;
  // Logger
  std::shared_ptr<core::logging::Logger> logger_;
  // Home location for this executable
  std::filesystem::path minifi_home_;

  std::string name_;
};

}  // namespace org::apache::nifi::minifi
