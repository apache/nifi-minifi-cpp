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
#ifndef __PROPERTIES_H__
#define __PROPERTIES_H__

#include <stdio.h>
#include <string>
#include <map>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include "core/logging/Logger.h"

#ifndef FILE_SEPARATOR
	#ifdef WIN32
		#define FILE_SEPARATOR '\\'
	#else
		#define FILE_SEPARATOR '/'
	#endif
#endif


namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Properties {
 public:
  Properties();

  virtual ~Properties() {

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
  bool has(std::string key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return (properties_.find(key) != properties_.end());
  }
  /**
   * Returns the config value by placing it into the referenced param value
   * @param key key to look up
   * @param value value in which to place the map's stored property value
   * @returns true if found, false otherwise.
   */
  bool get(const std::string &key, std::string &value);

  /**
   * Returns the config value by placing it into the referenced param value
   * Uses alternate_key if key is not found within the map.
   *
   * @param key key to look up
   * @param alternate_key is the secondary lookup key if key is not found
   * @param value value in which to place the map's stored property value
   * @returns true if found, false otherwise.
   */
  bool get(const std::string &key, const std::string &alternate_key, std::string &value);

  /**
   * Returns the configuration value or an empty string.
   * @return value corresponding to key or empty value.
   */
  int getInt(const std::string &key, int default_value);

  // Parse one line in configure file like key=value
  bool parseConfigureFileLine(char *buf, std::string &prop_key, std::string &prop_value);
  // Load Configure File
  void loadConfigureFile(const char *fileName);
  // Set the determined MINIFI_HOME
  void setHome(std::string minifiHome) {
    minifi_home_ = minifiHome;
  }

  std::vector<std::string> getConfiguredKeys() {
    std::vector<std::string> keys;
    for (auto &property : properties_) {
      keys.push_back(property.first);
    }
    return keys;
  }

  // Get the determined MINIFI_HOME
  std::string getHome() {
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
  std::mutex mutex_;
  // Logger
  std::shared_ptr<minifi::core::logging::Logger> logger_;
  // Home location for this executable
  std::string minifi_home_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
