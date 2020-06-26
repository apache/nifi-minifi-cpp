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
#ifndef LIBMINIFI_INCLUDE_PROPERTIES_PROPERTIES_H_
#define LIBMINIFI_INCLUDE_PROPERTIES_PROPERTIES_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Properties {
 public:
  Properties(const std::string& name = ""); // NOLINT

  virtual ~Properties() = default;

  virtual const std::string& getName() const {
    return name_;
  }

  // Clear the load config
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_.clear();
  }
  // Set the config value
  void set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_[key] = value;
    dirty_ = true;
  }
  // Check whether the config value existed
  bool has(std::string key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.count(key) > 0;
  }
  /**
   * Returns the config value by placing it into the referenced param value
   * @param key key to look up
   * @param value value in which to place the map's stored property value
   * @returns true if found, false otherwise.
   */
  bool get(const std::string &key, std::string &value) const;

  /**
   * Returns the config value by placing it into the referenced param value
   * Uses alternate_key if key is not found within the map.
   *
   * @param key key to look up
   * @param alternate_key is the secondary lookup key if key is not found
   * @param value value in which to place the map's stored property value
   * @returns true if found, false otherwise.
   */
  bool get(const std::string &key, const std::string &alternate_key, std::string &value) const;

  /**
   * Returns the configuration value or an empty string.
   * @return value corresponding to key or empty value.
   */
  int getInt(const std::string &key, int default_value) const;

  // Parse one line in configure file like key=value
  bool parseConfigureFileLine(char *buf, std::string &prop_key, std::string &prop_value);

  /**
   * Load configure file
   * @param fileName path of the configuration file RELATIVE to MINIFI_HOME set by setHome()
   */
  void loadConfigureFile(const char *fileName);

  // Set the determined MINIFI_HOME
  void setHome(std::string minifiHome) {
    minifi_home_ = minifiHome;
  }

  std::vector<std::string> getConfiguredKeys() const {
    std::vector<std::string> keys;
    for (auto &property : properties_) {
      keys.push_back(property.first);
    }
    return keys;
  }

  // Get the determined MINIFI_HOME
  std::string getHome() const {
    return minifi_home_;
  }
  // Parse Command Line
  void parseCommandLine(int argc, char **argv);

  bool persistProperties();

 protected:
  bool validateConfigurationFile(const std::string &file);

  std::map<std::string, std::string> properties_;


 private:
  std::atomic<bool> dirty_;

  std::string properties_file_;

  // Mutex for protection
  mutable std::mutex mutex_;
  // Logger
  std::shared_ptr<minifi::core::logging::Logger> logger_;
  // Home location for this executable
  std::string minifi_home_;

  std::string name_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_PROPERTIES_PROPERTIES_H_
