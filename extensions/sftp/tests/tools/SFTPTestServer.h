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
#pragma once

#include <cstdint>
#include <string>
#include <memory>

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

std::filesystem::path get_sftp_test_dir();

class SFTPTestServer {
 public:
  SFTPTestServer(std::filesystem::path working_directory,
      const std::filesystem::path& host_key_file = "resources/host.pem",
      const std::filesystem::path& jar_path = "tools/sftp-test-server/target/SFTPTestServer-1.0.0.jar");
  ~SFTPTestServer();

  bool start();
  bool stop();
  uint16_t getPort() const;

 private:
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<SFTPTestServer>::getLogger();

  std::filesystem::path host_key_file_;
  std::filesystem::path jar_path_;
  std::filesystem::path working_directory_;
  bool started_;
  std::filesystem::path port_file_path_;
  uint16_t port_;
#ifndef WIN32
  pid_t server_pid_;
#endif
};
