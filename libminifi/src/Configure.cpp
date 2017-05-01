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
#include "properties/Configure.h"
#include <string>
#include "utils/StringUtils.h"
#include "core/Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const char *Configure::nifi_flow_configuration_file =
    "nifi.flow.configuration.file";
const char *Configure::nifi_administrative_yield_duration =
    "nifi.administrative.yield.duration";
const char *Configure::nifi_bored_yield_duration = "nifi.bored.yield.duration";
const char *Configure::nifi_graceful_shutdown_seconds =
    "nifi.flowcontroller.graceful.shutdown.period";
const char *Configure::nifi_log_level = "nifi.log.level";
const char *Configure::nifi_server_name = "nifi.server.name";
const char *Configure::nifi_configuration_class_name =
    "nifi.flow.configuration.class.name";
const char *Configure::nifi_flow_repository_class_name =
    "nifi.flow.repository.class.name";
const char *Configure::nifi_provenance_repository_class_name =
    "nifi.provenance.repository.class.name";
const char *Configure::nifi_server_port = "nifi.server.port";
const char *Configure::nifi_server_report_interval =
    "nifi.server.report.interval";
const char *Configure::nifi_provenance_repository_max_storage_size =
    "nifi.provenance.repository.max.storage.size";
const char *Configure::nifi_provenance_repository_max_storage_time =
    "nifi.provenance.repository.max.storage.time";
const char *Configure::nifi_provenance_repository_directory_default =
    "nifi.provenance.repository.directory.default";
const char *Configure::nifi_flowfile_repository_max_storage_size =
    "nifi.flowfile.repository.max.storage.size";
const char *Configure::nifi_flowfile_repository_max_storage_time =
    "nifi.flowfile.repository.max.storage.time";
const char *Configure::nifi_flowfile_repository_directory_default =
    "nifi.flowfile.repository.directory.default";
const char *Configure::nifi_remote_input_secure = "nifi.remote.input.secure";
const char *Configure::nifi_security_need_ClientAuth =
    "nifi.security.need.ClientAuth";
const char *Configure::nifi_security_client_certificate =
    "nifi.security.client.certificate";
const char *Configure::nifi_security_client_private_key =
    "nifi.security.client.private.key";
const char *Configure::nifi_security_client_pass_phrase =
    "nifi.security.client.pass.phrase";
const char *Configure::nifi_security_client_ca_certificate =
    "nifi.security.client.ca.certificate";

#define BUFFER_SIZE 512

// Get the config value
bool Configure::get(std::string key, std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    value = it->second;
    return true;
  } else {
    return false;
  }
}

// Parse one line in configure file like key=value
void Configure::parseConfigureFileLine(char *buf) {
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
void Configure::loadConfigureFile(const char *fileName) {
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
void Configure::parseCommandLine(int argc, char **argv) {
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
