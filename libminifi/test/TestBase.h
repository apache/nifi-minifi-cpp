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

#ifndef LIBMINIFI_TEST_TESTBASE_H_
#define LIBMINIFI_TEST_TESTBASE_H_
#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include "ResourceClaim.h"
#include "catch.hpp"
#include <vector>
#include "core/logging/LogAppenders.h"
#include "core/logging/Logger.h"
#include "core/Core.h"
#include "properties/Configure.h"

class LogTestController {
 public:
  LogTestController(const std::string level = "debug") {
    logging::Logger::getLogger()->setLogLevel(level);
  }

  void enableDebug() {
    logging::Logger::getLogger()->setLogLevel("debug");
  }

  ~LogTestController() {
    logging::Logger::getLogger()->setLogLevel(logging::LOG_LEVEL_E::info);
  }
};

class TestController {
 public:

  TestController()
      : log("info") {
    minifi::ResourceClaim::default_directory_path = const_cast<char*>("./");
  }

  ~TestController() {
    for (auto dir : directories) {
      DIR *created_dir;
      struct dirent *dir_entry;
      created_dir = opendir(dir);
      if (created_dir != NULL) {
        while ((dir_entry = readdir(created_dir)) != NULL) {
          if (dir_entry->d_name[0] != '.') {

            std::string file(dir);
            file += "/";
            file += dir_entry->d_name;
            unlink(file.c_str());
          }
        }
      }
      closedir(created_dir);
      rmdir(dir);
    }
  }

  void setDebugToConsole(std::shared_ptr<org::apache::nifi::minifi::Configure> configure) {
    std::ostringstream oss;
    std::unique_ptr<logging::BaseLogger> outputLogger = std::unique_ptr<
        logging::BaseLogger>(
        new org::apache::nifi::minifi::core::logging::OutputStreamAppender(std::cout, configure));
    std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
    logger->updateLogger(std::move(outputLogger));
  }

  void setNullAppender() {

    std::unique_ptr<logging::BaseLogger> outputLogger = std::unique_ptr<
        logging::BaseLogger>(
        new org::apache::nifi::minifi::core::logging::NullAppender());
    std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
    logger->updateLogger(std::move(outputLogger));
  }

  void enableDebug() {
    log.enableDebug();
  }

  char *createTempDirectory(char *format) {
    char *dir = mkdtemp(format);
    directories.push_back(dir);
    return dir;
  }

 protected:
  LogTestController log;
  std::vector<char*> directories;

};

#endif /* LIBMINIFI_TEST_TESTBASE_H_ */
