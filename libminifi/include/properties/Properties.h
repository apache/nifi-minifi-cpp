// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/core/logging/Logger.h"
#include "utils/ChecksumCalculator.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/properties/Properties.h"

namespace org::apache::nifi::minifi::properties {

std::vector<std::filesystem::path> getExtraPropertiesFileNames(const std::filesystem::path& extra_properties_files_dir, const std::shared_ptr<core::logging::Logger>& logger);

std::filesystem::path extraPropertiesFilesDirName(const std::filesystem::path& base_properties_file);

}  // namespace org::apache::nifi::minifi::properties

namespace org::apache::nifi::minifi {

class PropertiesImpl : public virtual Properties {
  struct PropertyValue {
    std::string persisted_value;
    std::string active_value;
    bool need_to_persist_new_value{false};
  };

 public:
  enum class PersistTo { SingleFile, MultipleFiles };

  explicit PropertiesImpl(PersistTo persist_to, std::string name = "");

  ~PropertiesImpl() override = default;

  static constexpr std::string_view C2PropertiesFileName = "90_c2.properties";

  const std::string& getName() const override {
    return name_;
  }

  void clear() override {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_.clear();
  }
  void set(const std::string& key, const std::string& value) override {
    set(key, value, PropertyChangeLifetime::PERSISTENT);
  }
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

  void loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix = "") override;

  std::vector<std::string> getConfiguredKeys() const override {
    std::vector<std::string> keys;
    for (auto &property : properties_) {
      keys.push_back(property.first);
    }
    return keys;
  }

  bool commitChanges() override;

  utils::ChecksumCalculator& getChecksumCalculator() override { return checksum_calculator_; }

  std::filesystem::path getFilePath() const override;

  std::map<std::string, std::string> getProperties() const override;

  std::filesystem::path extraPropertiesFilesDirName() const override;

 private:
  void setPropertiesFromFile(const std::filesystem::path& properties_file, std::string_view prefix);

  std::map<std::string, PropertyValue> properties_;
  bool dirty_{false};
  std::filesystem::path base_properties_file_;
  std::vector<std::filesystem::path> properties_files_;
  utils::ChecksumCalculator checksum_calculator_;
  mutable std::mutex mutex_;
  std::shared_ptr<core::logging::Logger> logger_;
  PersistTo persist_to_;
  std::string name_;
};

}  // namespace org::apache::nifi::minifi
