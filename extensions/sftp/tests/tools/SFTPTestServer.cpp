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

#include "SFTPTestServer.h"
#include <vector>
#include <thread>
#include <exception>

#ifndef WIN32
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstring>
#include <cerrno>
#endif

#include "utils/file/FileUtils.h"

SFTPTestServer::SFTPTestServer(const std::string& working_directory,
                               const std::string& host_key_file /*= "resources/host.pem"*/,
                               const std::string& jar_path /*= "tools/sftp-test-server/target/SFTPTestServer-1.0.0.jar"*/)
    : logger_(logging::LoggerFactory<SFTPTestServer>::getLogger()),
      working_directory_(working_directory),
      started_(false),
      port_(0U)
#ifndef WIN32
      , server_pid_(-1)
#endif
{
  host_key_file_ = utils::file::FileUtils::concat_path(get_sftp_test_dir(), host_key_file);
  jar_path_ = utils::file::FileUtils::concat_path(get_sftp_test_dir(), jar_path);
}

SFTPTestServer::~SFTPTestServer() {
  try {
    this->stop();
  } catch (...) {
  }
}

bool SFTPTestServer::start() {
  if (started_) {
    return true;
  }
#ifdef WIN32
  throw std::runtime_error("Not implemented");
#else
  /* Delete possible previous port.txt */
  port_file_path_ = utils::file::FileUtils::concat_path(working_directory_, "port.txt");
  if (!port_file_path_.empty()) {
    logger_->log_debug("Deleting port file %s", port_file_path_.c_str());
    ::unlink(port_file_path_.c_str());
  }

  auto server_log_file_path = utils::file::FileUtils::concat_path(working_directory_, "log.txt");

  /* fork */
  pid_t pid = fork();
  if (pid == 0) {
    /* execv */
    std::vector<char*> args(4U);
    args[0] = strdup("/bin/sh");
    args[1] = strdup("-c");
    args[2] = strdup(("exec java -Djava.security.egd=file:/dev/./urandom -jar " + jar_path_ + " -w " + working_directory_ + " -k " + host_key_file_ + " >" + server_log_file_path + " 2>&1").c_str());
    args[3] = nullptr;
    execv("/bin/sh", args.data());
    std::cerr << "Failed to start server, errno: " << strerror(errno) << std::endl;
    exit(-1);
  } else if (pid < 0) {
    logger_->log_error("Failed to fork, error: %s", strerror(errno));
    return false;
  } else {
    server_pid_ = pid;

    /* Wait for port.txt to be created */
    for (size_t i = 0; i < 15; i++) {
      std::ifstream port_file(port_file_path_);
      if (port_file.is_open() && port_file.good()) {
        uint16_t port;
        if (port_file >> port) {
          port_ = port;
          started_ = true;
          logger_->log_debug("Found port file after %zu seconds", i);
          return true;
        }
      }
      logger_->log_debug("Could not find port file after %zu seconds", i);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* We could not find the port file, but the server has been started. Try to kill it. */
    this->stop();
  }
#endif

  return false;
}

bool SFTPTestServer::stop() {
#ifdef WIN32
  throw std::runtime_error("Not implemented");
#else
  if (server_pid_ != -1) {
    if (::kill(server_pid_, SIGTERM) != 0) {
      logger_->log_error("Failed to kill child process, error: %s", strerror(errno));
      return false;
    }
    int wstatus;
    if (::waitpid(server_pid_, &wstatus, 0) == -1) {
      logger_->log_error("Failed to waitpid for child process, error: %s", strerror(errno));
      return false;
    }
  }
  if (!port_file_path_.empty()) {
    logger_->log_debug("Deleting port file %s", port_file_path_.c_str());
    ::unlink(port_file_path_.c_str());
  }
#endif
  server_pid_ = -1;
  started_ = false;
  port_file_path_ = "";
  return true;
}

uint16_t SFTPTestServer::getPort() {
  return port_;
}

std::string get_sftp_test_dir() {
  return utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "sftp-test");
}
