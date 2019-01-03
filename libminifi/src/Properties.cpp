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
#include <string>
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

#ifndef FILE_SEPARATOR_C
#ifdef WIN32
#define FILE_SEPARATOR_C '\\'
#else
#define FILE_SEPARATOR_C '/'
#endif
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

#define TRACE_BUFFER_SIZE 512

Properties::Properties()
    : logger_(logging::LoggerFactory<Properties>::getLogger()) {
}

// Get the config value
bool Properties::get(const std::string &key, std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    value = it->second;
    return true;
  } else {
    return false;
  }
}

bool Properties::get(const std::string &key, const std::string &alternate_key, std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it == properties_.end()) {
    it = properties_.find(alternate_key);
    if (it != properties_.end()) {
      logger_->log_warn("%s is an alternate property that may not be supported in future releases. Please use %s instead.", alternate_key, key);
    }
  }

  if (it != properties_.end()) {
    value = it->second;
    return true;
  } else {
    return false;
  }
}

int Properties::getInt(const std::string &key, int default_value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    return std::atol(it->second.c_str());
  } else {
    return default_value;
  }
}

// Parse one line in configure file like key=value
bool Properties::parseConfigureFileLine(char *buf, std::string &prop_key, std::string &prop_value) {
  char *line = buf;

  while ((line[0] == ' ') || (line[0] == '\t'))
    ++line;

  char first = line[0];
  if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n') || (first == '=')) {
    return true;
  }

  char *equal = strchr(line, '=');
  if (equal == NULL) {
    return false;  // invalid property as this is not a comment or property line
  }

  equal[0] = '\0';
  std::string key = line;

  equal++;
  while ((equal[0] == ' ') || (equal[0] == '\t'))
    ++equal;

  first = equal[0];
  if ((first == '\0') || (first == '\r') || (first == '\n')) {
    return true;  // empty properties are okay
  }

  std::string value = equal;
  value = org::apache::nifi::minifi::utils::StringUtils::replaceEnvironmentVariables(value);
  prop_key = org::apache::nifi::minifi::utils::StringUtils::trimRight(key);
  prop_value = org::apache::nifi::minifi::utils::StringUtils::trimRight(value);
  return true;
}

// Load Configure File
void Properties::loadConfigureFile(const char *fileName) {
  std::string adjustedFilename;
  if (fileName) {
    // perform a naive determination if this is a relative path
    if (fileName[0] != FILE_SEPARATOR_C) {
      adjustedFilename = adjustedFilename + getHome() + FILE_SEPARATOR_C + fileName;
    } else {
      if (adjustedFilename.empty()) {
        adjustedFilename = getHome();
      }
      adjustedFilename += fileName;
    }
  }
  char *path = NULL;
#ifndef WIN32
  char full_path[PATH_MAX];
  path = realpath(adjustedFilename.c_str(), full_path);
#else
  path = const_cast<char*>(adjustedFilename.c_str());
#endif
  logger_->log_info("Using configuration file located at %s, from %s", path, fileName);

  properties_file_ = path;

  std::ifstream file(path, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", path);
    return;
  }
  this->clear();

  char buf[TRACE_BUFFER_SIZE];
  for (file.getline(buf, TRACE_BUFFER_SIZE); file.good(); file.getline(buf, TRACE_BUFFER_SIZE)) {
    std::string key, value;
    if (parseConfigureFileLine(buf, key, value)) {
      set(key, value);
    }
  }
  dirty_ = false;
}

bool Properties::validateConfigurationFile(const std::string &configFile) {
  std::ifstream file(configFile, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", configFile);
    return false;
  }

  char buf[TRACE_BUFFER_SIZE];
  for (file.getline(buf, TRACE_BUFFER_SIZE); file.good(); file.getline(buf, TRACE_BUFFER_SIZE)) {
    std::string key, value;
    if (!parseConfigureFileLine(buf, key, value)) {
      return false;
    }
  }
  return true;
}

bool Properties::persistProperties() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_) {
    logger_->log_info("Attempt to persist, but properties are not updated");
    return true;
  }
  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", properties_file_);
    return false;
  }

  std::map<std::string, std::string> properties_copy = properties_;

  std::string new_file = properties_file_ + ".new";

  std::ofstream output_file(new_file, std::ios::out);

  char buf[TRACE_BUFFER_SIZE];
  for (file.getline(buf, TRACE_BUFFER_SIZE); file.good(); file.getline(buf, TRACE_BUFFER_SIZE)) {
    char *line = buf;

    char first = line[0];

    if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n') || (first == '=')) {
      // persist comments and newlines
      output_file << line << std::endl;
      continue;
    }

    char *equal = strchr(line, '=');
    if (equal == NULL) {
      output_file << line << std::endl;
      continue;
    }

    equal[0] = '\0';
    std::string key = line;

    equal++;
    while ((equal[0] == ' ') || (equal[0] == '\t'))
      ++equal;

    first = equal[0];
    if ((first == '\0') || (first == '\r') || (first == '\n')) {
      output_file << line << std::endl;
      continue;
    }

    std::string value = equal;
    value = org::apache::nifi::minifi::utils::StringUtils::replaceEnvironmentVariables(value);
    key = org::apache::nifi::minifi::utils::StringUtils::trimRight(key);
    value = org::apache::nifi::minifi::utils::StringUtils::trimRight(value);
    auto hasIt = properties_copy.find(key);
    if (hasIt != properties_copy.end() && !value.empty()) {
      output_file << key << "=" << hasIt->second << std::endl;
    }
    properties_copy.erase(key);
  }

  for (const auto &kv : properties_copy) {
    if (!kv.first.empty() && !kv.second.empty())
      output_file << kv.first << "=" << kv.second << std::endl;
  }
  output_file.close();

  if (validateConfigurationFile(new_file)) {
    const std::string backup = properties_file_ + ".bak";
    if (!utils::file::FileUtils::copy_file(properties_file_, backup) && !utils::file::FileUtils::copy_file(new_file, properties_file_)) {
      logger_->log_info("Persisted %s", properties_file_);
      return true;
    } else {
      logger_->log_error("Could not update %s", properties_file_);
    }
  }

  dirty_ = false;

  return false;
}

// Parse Command Line
void Properties::parseCommandLine(int argc, char **argv) {
  int i;
  bool keyFound = false;
  std::string key, value;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      keyFound = true;
      key = &argv[i][1];
      continue;
    }
    if (keyFound) {
      value = argv[i];
      set(key, value);
      keyFound = false;
    }
  }
  return;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
