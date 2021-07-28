/**
 * @file ExecuteProcess.h
 * ExecuteProcess class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXECUTEPROCESS_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXECUTEPROCESS_H_

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifndef WIN32
#include <sys/wait.h>

#endif
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "FlowFileRecord.h"
#include "io/BaseStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
#ifndef WIN32

// ExecuteProcess Class
class ExecuteProcess : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  ExecuteProcess(const std::string& name, const utils::Identifier& uuid = {}) // NOLINT
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ExecuteProcess>::getLogger()) {
    _redirectErrorStream = false;
    _batchDuration = 0;
    _workingDir = ".";
    _processRunning = false;
    _pid = 0;
  }
  // Destructor
  virtual ~ExecuteProcess() {
    if (_processRunning && _pid > 0)
      kill(_pid, SIGTERM);
  }
  // Processor Name
  static constexpr char const* ProcessorName = "ExecuteProcess";
  // Supported Properties
  static core::Property Command;
  static core::Property CommandArguments;
  static core::Property WorkingDir;
  static core::Property BatchDuration;
  static core::Property RedirectErrorStream;
  // Supported Relationships
  static core::Relationship Success;

  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(char *data, uint64_t size)
        : _data(data),
          _dataSize(size) {
    }
    char *_data;
    uint64_t _dataSize;
    // void process(std::ofstream *stream) {
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (!_data || _dataSize <= 0) return 0;
      const auto write_ret = stream->write(reinterpret_cast<uint8_t*>(_data), _dataSize);
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }
  };

 public:
  // OnTrigger method, implemented by NiFi ExecuteProcess
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi ExecuteProcess
  virtual void initialize(void);

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Property
  std::string _command;
  std::string _commandArgument;
  std::string _workingDir;
  int64_t _batchDuration;
  bool _redirectErrorStream;
  // Full command
  std::string _fullCommand;
  // whether the process is running
  bool _processRunning;
  int _pipefd[2];
  pid_t _pid;
};

#endif
}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXECUTEPROCESS_H_
