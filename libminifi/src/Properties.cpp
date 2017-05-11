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
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

#define BUFFER_SIZE 512

Properties::Properties() : logger_(logging::LoggerFactory<Properties>::getLogger()) {}

// Get the config value
bool Properties::get(std::string key, std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

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
void Properties::parseConfigureFileLine(char *buf) {
  char *line = buf;

  while ((line[0] == ' ') || (line[0] == '\t'))
    ++line;

  char first = line[0];
  if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n')
      || (first == '=')) {
    return;
  }

  char *equal = strchr(line, '=');
  if (equal == NULL) {
    return;
  }

  equal[0] = '\0';
  std::string key = line;

  equal++;
  while ((equal[0] == ' ') || (equal[0] == '\t'))
    ++equal;

  first = equal[0];
  if ((first == '\0') || (first == '\r') || (first == '\n')) {
    return;
  }

  std::string value = equal;
  key = org::apache::nifi::minifi::utils::StringUtils::trimRight(key);
  value = org::apache::nifi::minifi::utils::StringUtils::trimRight(value);
  set(key, value);
}

// Load Configure File
void Properties::loadConfigureFile(const char *fileName) {
  std::string adjustedFilename;
  if (fileName) {
    // perform a naive determination if this is a relative path
    if (fileName[0] != '/') {
      adjustedFilename = adjustedFilename + getHome() + "/"
          + fileName;
    } else {
      adjustedFilename += fileName;
    }
  }
  char *path = NULL;
  char full_path[PATH_MAX];
  path = realpath(adjustedFilename.c_str(), full_path);
  logger_->log_info("Using configuration file located at %s", path);

  std::ifstream file(path, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", path);
    return;
  }
  this->clear();

  char buf[BUFFER_SIZE];
  for (file.getline(buf, BUFFER_SIZE); file.good();
      file.getline(buf, BUFFER_SIZE)) {
    parseConfigureFileLine(buf);
  }
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
