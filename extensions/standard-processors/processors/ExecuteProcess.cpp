/**
 * @file ExecuteProcess.cpp
 * ExecuteProcess class implementation
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
#include "ExecuteProcess.h"
#include <memory>
#include <string>
#include <iomanip>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/TypedValues.h"
#include "utils/gsl.h"
#include "utils/Environment.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

void ExecuteProcess::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ExecuteProcess::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*session_factory*/) {
  gsl_Expects(context);
  std::string value;
  if (context->getProperty(Command, value)) {
    command_ = value;
  }
  if (context->getProperty(CommandArguments, value)) {
    command_argument_ = value;
  }
  if (auto working_dir_str = context->getProperty(WorkingDir)) {
    working_dir_ = *working_dir_str;
  }
  if (auto batch_duration = context->getProperty<core::TimePeriodValue>(BatchDuration)) {
    batch_duration_ = batch_duration->getMilliseconds();
    logger_->log_debug("Setting batch duration to %d milliseconds", batch_duration_.count());
  }
  if (context->getProperty(RedirectErrorStream, value)) {
    redirect_error_stream_ = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }
  full_command_ = command_ + " " + command_argument_;
}

std::vector<std::string> ExecuteProcess::readArgs() const {
  std::vector<std::string> args;
  std::stringstream input_stream{full_command_};
  while (input_stream) {
    std::string word;
    input_stream >> std::quoted(word);
    if (!word.empty()) {
      args.push_back(word);
    }
  }

  return args;
}

void ExecuteProcess::executeProcessForkFailed() {
  logger_->log_error("Execute Process fork failed");
  close(pipefd_[0]);
  close(pipefd_[1]);
  yield();
}

void ExecuteProcess::executeChildProcess() {
  std::vector<char*> argv;
  auto args = readArgs();
  argv.reserve(args.size() + 1);
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  static constexpr int STDOUT = 1;
  static constexpr int STDERR = 2;
  if (dup2(pipefd_[1], STDOUT) < 0) {  // points pipefd at file descriptor
    logger_->log_error("Failed to point pipe at file descriptor");
    exit(1);
  }
  if (redirect_error_stream_ && dup2(pipefd_[1], STDERR) < 0) {
    logger_->log_error("Failed to redirect error stream of the executed process to the output stream");
    exit(1);
  }
  close(pipefd_[0]);
  if (execvp(argv[0], argv.data()) < 0) {
    logger_->log_error("Failed to execute child process");
    exit(1);
  }
  exit(0);
}

void ExecuteProcess::readOutputInBatches(core::ProcessSession& session) {
  while (true) {
    std::this_thread::sleep_for(batch_duration_);
    char buffer[4096];
    const auto num_read = read(pipefd_[0], buffer, sizeof(buffer));
    if (num_read <= 0) {
      break;
    }
    logger_->log_debug("Execute Command Respond %zd", num_read);
    auto flow_file = session.create();
    if (!flow_file) {
      logger_->log_error("Flow file could not be created!");
      continue;
    }
    flow_file->addAttribute("command", command_);
    flow_file->addAttribute("command.arguments", command_argument_);
    session.writeBuffer(flow_file, gsl::make_span(buffer, gsl::narrow<size_t>(num_read)));
    session.transfer(flow_file, Success);
    session.commit();
  }
}

bool ExecuteProcess::writeToFlowFile(core::ProcessSession& session, std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) const {
  if (!flow_file) {
    flow_file = session.create();
    if (!flow_file) {
      logger_->log_error("Flow file could not be created!");
      return false;
    }
    flow_file->addAttribute("command", command_);
    flow_file->addAttribute("command.arguments", command_argument_);
    session.writeBuffer(flow_file, buffer);
  } else {
    session.appendBuffer(flow_file, buffer);
  }
  return true;
}

void ExecuteProcess::readOutput(core::ProcessSession& session) {
  char buffer[4096];
  char *buf_ptr = buffer;
  size_t read_to_buffer = 0;
  std::shared_ptr<core::FlowFile> flow_file;
  auto num_read = read(pipefd_[0], buf_ptr, sizeof(buffer));
  while (num_read > 0) {
    if (num_read == static_cast<ssize_t>(sizeof(buffer) - read_to_buffer)) {
      // we reach the max buffer size
      logger_->log_debug("Execute Command Max Respond %zu", sizeof(buffer));
      if (!writeToFlowFile(session, flow_file, buffer)) {
        continue;
      }
      // Rewind
      read_to_buffer = 0;
      buf_ptr = buffer;
    } else {
      read_to_buffer += num_read;
      buf_ptr += num_read;
    }
    num_read = read(pipefd_[0], buf_ptr, (sizeof(buffer) - read_to_buffer));
  }

  if (read_to_buffer > 0) {
    logger_->log_debug("Execute Command Respond %zu", read_to_buffer);
    // child exits and close the pipe
    const auto buffer_span = gsl::make_span(buffer, read_to_buffer);
    if (!writeToFlowFile(session, flow_file, buffer_span)) {
      return;
    }
  }
  if (flow_file) {
    session.transfer(flow_file, Success);
  }
}

void ExecuteProcess::collectChildProcessOutput(core::ProcessSession& session) {
  // the parent isn't going to write to the pipe
  close(pipefd_[1]);
  if (batch_duration_ > 0ms) {
    readOutputInBatches(session);
  } else {
    readOutput(session);
  }

  int status = 0;
  wait(&status);
  if (WIFEXITED(status)) {
    logger_->log_info("Execute Command Complete %s status %d pid %d", full_command_, WEXITSTATUS(status), pid_);
  } else {
    logger_->log_info("Execute Command Complete %s status %d pid %d", full_command_, WTERMSIG(status), pid_);
  }

  close(pipefd_[0]);
  pid_ = 0;
}

void ExecuteProcess::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  gsl_Expects(context && session);
  if (full_command_.length() == 0) {
    yield();
    return;
  }
  std::error_code current_path_error;
  std::filesystem::current_path(working_dir_, current_path_error);
  if (current_path_error) {
    yield();
    return;
  }
  logger_->log_info("Execute Command %s", full_command_);

  if (pipe(pipefd_) == -1) {
    yield();
    return;
  }
  switch (pid_ = fork()) {
    case -1:
      executeProcessForkFailed();
      break;
    case 0:  // this is the code the child runs
      executeChildProcess();
      break;
    default:  // this is the code the parent runs
      collectChildProcessOutput(*session);
      break;
  }
}

REGISTER_RESOURCE(ExecuteProcess, Processor);

}  // namespace org::apache::nifi::minifi::processors
#endif
