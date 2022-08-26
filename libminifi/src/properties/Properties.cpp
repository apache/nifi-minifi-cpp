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
#include "properties/Properties.h"
#include <fstream>
#include <string>
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/PropertiesFile.h"

namespace org::apache::nifi::minifi {

Properties::Properties(std::string name)
    : logger_(core::logging::LoggerFactory<Properties>::getLogger()),
    name_(std::move(name)) {
}

// Get the config value
bool Properties::getString(const std::string &key, std::string &value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    value = it->second.active_value;
    return true;
  } else {
    return false;
  }
}

std::optional<std::string> Properties::getString(const std::string& key) const {
  std::string result;
  const bool found = getString(key, result);
  if (found) {
    return result;
  } else {
    return std::nullopt;
  }
}

int Properties::getInt(const std::string &key, int default_value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  return it != properties_.end() ? std::stoi(it->second.active_value) : default_value;
}

// Load Configure File
void Properties::loadConfigureFile(const char *fileName) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (fileName == nullptr) {
    logger_->log_error("Configuration file path for %s is a nullptr!", getName().c_str());
    return;
  }

  properties_file_ = utils::file::getFullPath(utils::file::FileUtils::concat_path(getHome(), fileName));
  if (properties_file_.empty()) {
    logger_->log_warn("Configuration file '%s' does not exist, so it could not be loaded.", fileName);
    return;
  }

  logger_->log_info("Using configuration file to load configuration for %s from %s (located at %s)",
                    getName().c_str(), fileName, properties_file_);

  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", properties_file_);
    return;
  }
  properties_.clear();
  for (const auto& line : PropertiesFile{file}) {
    auto persisted_value = line.getValue();
    auto value = utils::StringUtils::replaceEnvironmentVariables(persisted_value);
    properties_[line.getKey()] = {persisted_value, value, false};
  }
  checksum_calculator_.setFileLocation(properties_file_);
  dirty_ = false;
}

std::string Properties::getFilePath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return properties_file_;
}

bool Properties::commitChanges() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_) {
    logger_->log_info("Attempt to persist, but properties are not updated");
    return true;
  }
  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file) {
    logger_->log_error("load configure file failed %s", properties_file_);
    return false;
  }

  std::string new_file = properties_file_ + ".new";

  PropertiesFile current_content{file};
  for (const auto& prop : properties_) {
    if (!prop.second.need_to_persist_new_value) {
      continue;
    }
    if (current_content.hasValue(prop.first)) {
      current_content.update(prop.first, prop.second.persisted_value);
    } else {
      current_content.append(prop.first, prop.second.persisted_value);
    }
  }

  try {
    current_content.writeTo(new_file);
  } catch (const std::exception&) {
    logger_->log_error("Could not update %s", properties_file_);
    return false;
  }

  const std::string backup = properties_file_ + ".bak";
  if (utils::file::FileUtils::copy_file(properties_file_, backup) == 0 && utils::file::FileUtils::copy_file(new_file, properties_file_) == 0) {
    logger_->log_info("Persisted %s", properties_file_);
    checksum_calculator_.invalidateChecksum();
    dirty_ = false;
    return true;
  }

  logger_->log_error("Could not update %s", properties_file_);
  return false;
}

std::map<std::string, std::string> Properties::getProperties() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<std::string, std::string> properties;
  for (const auto& prop : properties_) {
    properties[prop.first] = prop.second.active_value;
  }
  return properties;
}

}  // namespace org::apache::nifi::minifi
