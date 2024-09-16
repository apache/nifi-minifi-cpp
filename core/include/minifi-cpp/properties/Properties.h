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
  virtual void setHome(std::filesystem::path minifiHome) = 0;
  virtual std::vector<std::string> getConfiguredKeys() const = 0;
  virtual std::filesystem::path getHome() const = 0;
  virtual bool commitChanges() = 0;
  virtual utils::ChecksumCalculator& getChecksumCalculator() = 0;
  virtual std::filesystem::path getFilePath() const = 0;
  virtual std::map<std::string, std::string> getProperties() const = 0;

  static std::shared_ptr<Properties> create();
};

}  // namespace org::apache::nifi::minifi
