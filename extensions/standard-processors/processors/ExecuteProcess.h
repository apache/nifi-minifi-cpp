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
#ifndef WIN32
#pragma once

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <utility>
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "FlowFileRecord.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

class ExecuteProcess : public core::Processor {
 public:
  explicit ExecuteProcess(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid),
        working_dir_(".") {
  }
  ~ExecuteProcess() override {
    if (pid_ > 0) {
      kill(pid_, SIGTERM);
    }
  }

  EXTENSIONAPI static constexpr const char* Description = "Runs an operating system command specified by the user and writes the output of that command to a FlowFile. "
      "If the command is expected to be long-running, the Processor can output the partial data on a specified interval. "
      "When this option is used, the output is expected to be in textual format, as it typically does not make sense to split binary data on arbitrary time-based intervals. "
      "This processor is not available on Windows systems.";

  EXTENSIONAPI static core::Property Command;
  EXTENSIONAPI static core::Property CommandArguments;
  EXTENSIONAPI static core::Property WorkingDir;
  EXTENSIONAPI static core::Property BatchDuration;
  EXTENSIONAPI static core::Property RedirectErrorStream;
  static auto properties() {
    return std::array{
      Command,
      CommandArguments,
      WorkingDir,
      BatchDuration,
      RedirectErrorStream
    };
  }

  EXTENSIONAPI static core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *session_factory) override;
  void initialize() override;

 private:
  std::vector<std::string> readArgs() const;
  void executeProcessForkFailed();
  void executeChildProcess();
  void collectChildProcessOutput(core::ProcessSession& session);
  void readOutputInBatches(core::ProcessSession& session);
  void readOutput(core::ProcessSession& session);
  bool writeToFlowFile(core::ProcessSession& session, std::shared_ptr<core::FlowFile>& flow_file, gsl::span<const char> buffer) const;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecuteProcess>::getLogger(uuid_);
  std::string command_;
  std::string command_argument_;
  std::filesystem::path working_dir_;
  std::chrono::milliseconds batch_duration_  = std::chrono::milliseconds(0);
  bool redirect_error_stream_ = false;
  std::string full_command_;
  int pipefd_[2]{};
  pid_t pid_{};
};

}  // namespace org::apache::nifi::minifi::processors
#endif
