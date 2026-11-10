// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#ifdef WIN32
// ignore the warning about inheriting via dominance from Properties
#pragma warning(disable : 4250)
#endif

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>

namespace org::apache::nifi::minifi {

namespace utils {
class ChecksumCalculator;
}  // namespace utils

enum class PropertyChangeLifetime {
  TRANSIENT,  // the changed value will not be committed to disk
  PERSISTENT  // the changed value will be written to the source file
};

class Properties {
 public:
  virtual ~Properties() = default;
  virtual const std::string& getName() const = 0;
  virtual void clear() = 0;
  virtual void set(const std::string& key, const std::string& value) = 0;
  virtual void set(const std::string &key, const std::string &value, PropertyChangeLifetime lifetime) = 0;
  virtual bool has(const std::string& key) const = 0;
  virtual bool getString(const std::string &key, std::string &value) const = 0;
  virtual int getInt(const std::string &key, int default_value) const = 0;
  virtual std::optional<std::string> getString(const std::string& key) const = 0;
  virtual void loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix = "") = 0;
  virtual std::vector<std::string> getConfiguredKeys() const = 0;
  virtual bool commitChanges() = 0;
  virtual utils::ChecksumCalculator& getChecksumCalculator() = 0;
  virtual std::filesystem::path getFilePath() const = 0;
  virtual std::map<std::string, std::string> getProperties() const = 0;
  virtual std::filesystem::path extraPropertiesFilesDirName() const = 0;

  static std::shared_ptr<Properties> create();
};

}  // namespace org::apache::nifi::minifi
