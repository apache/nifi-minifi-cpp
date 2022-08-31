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
#include "ExecuteProcess.h"
#include <cstring>
#include <memory>
#include <string>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/TypedValues.h"
#include "utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {
#ifndef WIN32
core::Property ExecuteProcess::Command(
    core::PropertyBuilder::createProperty("Command")
      ->withDescription("Specifies the command to be executed; if just the name of an executable is provided, it must be in the user's environment PATH.")
      ->withDefaultValue("")
      ->build());

core::Property ExecuteProcess::CommandArguments(
    core::PropertyBuilder::createProperty("Command Arguments")
      ->withDescription("The arguments to supply to the executable delimited by white space. White space can be escaped by enclosing it in double-quotes.")
      ->withDefaultValue("")
      ->build());

core::Property ExecuteProcess::WorkingDir(
    core::PropertyBuilder::createProperty("Working Directory")
      ->withDescription("The directory to use as the current working directory when executing the command")
      ->withDefaultValue("")
      ->build());

core::Property ExecuteProcess::BatchDuration(
    core::PropertyBuilder::createProperty("Batch Duration")
      ->withDescription("If the process is expected to be long-running and produce textual output, a batch duration can be specified.")
      ->withDefaultValue<core::TimePeriodValue>("0 sec")
      ->build());

core::Property ExecuteProcess::RedirectErrorStream(
    core::PropertyBuilder::createProperty("Redirect Error Stream")
      ->withDescription("If true will redirect any error stream output of the process to the output stream.")
      ->withDefaultValue<bool>(false)
      ->build());

core::Relationship ExecuteProcess::Success("success", "All created FlowFiles are routed to this relationship.");

void ExecuteProcess::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExecuteProcess::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*session_factory*/) {
  gsl_Expects(context);
  std::string value;
  if (context->getProperty(Command.getName(), value)) {
    command_ = value;
  }
  if (context->getProperty(CommandArguments.getName(), value)) {
    command_argument_ = value;
  }
  if (context->getProperty(WorkingDir.getName(), value)) {
    working_dir_ = value;
  }
  if (auto batch_duration = context->getProperty<core::TimePeriodValue>(BatchDuration)) {
    batch_duration_ = batch_duration->getMilliseconds();
    logger_->log_debug("Setting batch duration to %d milliseconds", batch_duration_.count());
  }
  if (context->getProperty(RedirectErrorStream.getName(), value)) {
    redirect_error_stream_ = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }
  full_command_ = command_ + " " + command_argument_;
}

bool ExecuteProcess::changeWorkdir() {
  if (working_dir_.length() > 0 && working_dir_ != ".") {
    if (chdir(working_dir_.c_str()) != 0) {
      logger_->log_error("Execute Command can not chdir %s", working_dir_);
      return false;
    }
  }
  return true;
}

void ExecuteProcess::populateArgArray(char** argv) {
  char *parameter = std::strtok(const_cast<char*>(full_command_.c_str()), " ");
  int argc = 0;
  while (parameter != 0 && argc < 64) {
    argv[argc] = parameter;
    parameter = std::strtok(nullptr, " ");
    argc++;
  }
  argv[argc] = nullptr;
}

void ExecuteProcess::executeProcessForkFailed() {
  logger_->log_error("Execute Process fork failed");
  close(pipefd_[0]);
  close(pipefd_[1]);
  yield();
}

void ExecuteProcess::executeChildProcess(char** argv) {
  const int STDOUT = 1;
  const int STDERR = 2;
  close(STDOUT);
  dup(pipefd_[1]);  // points pipefd at file descriptor
  if (redirect_error_stream_) {
    dup2(pipefd_[1], STDERR);
  }
  close(pipefd_[0]);
  execvp(argv[0], argv);
  exit(1);
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
      continue;
    }
    flow_file->addAttribute("command", command_);
    flow_file->addAttribute("command.arguments", command_argument_);
    session.writeBuffer(flow_file, gsl::make_span(buffer, gsl::narrow<size_t>(num_read)));
    session.transfer(flow_file, Success);
    session.commit();
  }
}

bool ExecuteProcess::writeToFlowFile(core::ProcessSession& session, std::shared_ptr<core::FlowFile>& flow_file, gsl::span<const char> buffer) {
  if (!flow_file) {
    flow_file = session.create();
    if (!flow_file) {
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
  size_t total_read = 0;
  std::shared_ptr<core::FlowFile> flow_file;
  auto num_read = read(pipefd_[0], buf_ptr, (sizeof(buffer) - total_read));
  while (num_read > 0) {
    if (num_read == static_cast<ssize_t>((sizeof(buffer) - total_read))) {
      // we reach the max buffer size
      logger_->log_debug("Execute Command Max Respond %zu", sizeof(buffer));
      if (!writeToFlowFile(session, flow_file, buffer)) {
        continue;
      }
      // Rewind
      total_read = 0;
      buf_ptr = buffer;
    } else {
      total_read += num_read;
      buf_ptr += num_read;
    }
    num_read = read(pipefd_[0], buf_ptr, (sizeof(buffer) - total_read));
  }

  if (total_read > 0) {
    logger_->log_debug("Execute Command Respond %zu", total_read);
    // child exits and close the pipe
    const auto buffer_span = gsl::make_span(buffer, total_read);
    if (!writeToFlowFile(session, flow_file, buffer_span)) {
      return;
    }
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
  if (!changeWorkdir()) {
    yield();
    return;
  }
  logger_->log_info("Execute Command %s", full_command_);
  char *argv[64];
  populateArgArray(argv);
  if (process_running_) {
    return;
  }

  if (pipe(pipefd_) == -1) {
    yield();
    return;
  }
  switch (pid_ = fork()) {
    case -1:
      executeProcessForkFailed();
      break;
    case 0:  // this is the code the child runs
      executeChildProcess(argv);
      break;
    default:  // this is the code the parent runs
      collectChildProcessOutput(*session);
      break;
  }
}

REGISTER_RESOURCE(ExecuteProcess, Processor);

#endif
}  // namespace org::apache::nifi::minifi::processors
