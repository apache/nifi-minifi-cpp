// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdint>
#include <string>
#include <memory>

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

#include "minifi-cpp/core/logging/Logger.h"
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
