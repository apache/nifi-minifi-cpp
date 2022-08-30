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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-result"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {
#ifndef WIN32
core::Property ExecuteProcess::Command(
    core::PropertyBuilder::createProperty("Command")->withDescription("Specifies the command to be executed; if just the name of an executable"
                                                                      " is provided, it must be in the user's environment PATH.")->supportsExpressionLanguage(true)->withDefaultValue("")->build());
core::Property ExecuteProcess::CommandArguments(
    core::PropertyBuilder::createProperty("Command Arguments")->withDescription("The arguments to supply to the executable delimited by white space. White "
                                                                                "space can be escaped by enclosing it in "
                                                                                "double-quotes.")->supportsExpressionLanguage(true)->withDefaultValue("")->build());
core::Property ExecuteProcess::WorkingDir(
    core::PropertyBuilder::createProperty("Working Directory")->withDescription("The directory to use as the current working directory when executing the command")->supportsExpressionLanguage(true)
        ->withDefaultValue("")->build());

core::Property ExecuteProcess::BatchDuration(
    core::PropertyBuilder::createProperty("Batch Duration")->withDescription("If the process is expected to be long-running and produce textual output, a "
                                                                             "batch duration can be specified.")->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

core::Property ExecuteProcess::RedirectErrorStream(
    core::PropertyBuilder::createProperty("Redirect Error Stream")->withDescription("If true will redirect any error stream output of the process to the output stream.")->withDefaultValue<bool>(false)
        ->build());

core::Relationship ExecuteProcess::Success("success", "All created FlowFiles are routed to this relationship.");

void ExecuteProcess::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExecuteProcess::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::string value;
  std::shared_ptr<core::FlowFile> flow_file;
  if (context->getProperty(Command, value, flow_file)) {
    this->command_ = value;
  }
  if (context->getProperty(CommandArguments, value, flow_file)) {
    this->command_argument_ = value;
  }
  if (context->getProperty(WorkingDir, value, flow_file)) {
    this->working_dir_ = value;
  }
  if (auto batch_duration = context->getProperty<core::TimePeriodValue>(BatchDuration)) {
    batch_duration_ = batch_duration->getMilliseconds();
    logger_->log_debug("Setting batch_duration_");
  }
  if (context->getProperty(RedirectErrorStream.getName(), value)) {
    redirect_error_stream_ =  org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }
  this->full_command_ = command_ + " " + command_argument_;
  if (full_command_.length() == 0) {
    yield();
    return;
  }
  if (working_dir_.length() > 0 && working_dir_ != ".") {
    // change to working directory
    if (chdir(working_dir_.c_str()) != 0) {
      logger_->log_error("Execute Command can not chdir %s", working_dir_);
      yield();
      return;
    }
  }
  logger_->log_info("Execute Command %s", full_command_);
  // split the command into array
  char *p = std::strtok(const_cast<char*>(full_command_.c_str()), " ");
  int argc = 0;
  char *argv[64];
  while (p != 0 && argc < 64) {
    argv[argc] = p;
    p = std::strtok(NULL, " ");
    argc++;
  }
  argv[argc] = NULL;
  int status;
  if (!process_running_) {
    process_running_ = true;
    // if the process has not launched yet
    // create the pipe
    if (pipe(pipefd_) == -1) {
      process_running_ = false;
      yield();
      return;
    }
    switch (pid_ = fork()) {
      case -1:
        logger_->log_error("Execute Process fork failed");
        process_running_ = false;
        close(pipefd_[0]);
        close(pipefd_[1]);
        yield();
        break;
      case 0:  // this is the code the child runs
        close(1);      // close stdout
        dup(pipefd_[1]);  // points pipefd at file descriptor
        if (redirect_error_stream_)
          // redirect stderr
          dup2(pipefd_[1], 2);
        close(pipefd_[0]);
        execvp(argv[0], argv);
        exit(1);
        break;
      default:  // this is the code the parent runs
        // the parent isn't going to write to the pipe
        close(pipefd_[1]);
        if (batch_duration_ > 0ms) {
          while (true) {
            std::this_thread::sleep_for(batch_duration_);
            char buffer[4096];
            const auto  numRead = read(pipefd_[0], buffer, sizeof(buffer));
            if (numRead <= 0)
              break;
            logger_->log_debug("Execute Command Respond %zd", numRead);
            auto flowFile = session->create();
            if (!flowFile)
              continue;
            flowFile->addAttribute("command", command_);
            flowFile->addAttribute("command.arguments", command_argument_);
            session->writeBuffer(flowFile, gsl::make_span(buffer, gsl::narrow<size_t>(numRead)));
            session->transfer(flowFile, Success);
            session->commit();
          }
        } else {
          char buffer[4096];
          char *bufPtr = buffer;
          size_t totalRead = 0;
          std::shared_ptr<core::FlowFile> flowFile = nullptr;
          while (true) {
            const auto numRead = read(pipefd_[0], bufPtr, (sizeof(buffer) - totalRead));
            if (numRead <= 0) {
              if (totalRead > 0) {
                logger_->log_debug("Execute Command Respond %zu", totalRead);
                // child exits and close the pipe
                const auto buffer_span = gsl::make_span(buffer, totalRead);
                if (!flowFile) {
                  flowFile = session->create();
                  if (!flowFile)
                    break;
                  flowFile->addAttribute("command", command_);
                  flowFile->addAttribute("command.arguments", command_argument_);
                  session->writeBuffer(flowFile, buffer_span);
                } else {
                  session->appendBuffer(flowFile, buffer_span);
                }
                session->transfer(flowFile, Success);
              }
              break;
            } else {
              if (numRead == static_cast<ssize_t>((sizeof(buffer) - totalRead))) {
                // we reach the max buffer size
                logger_->log_debug("Execute Command Max Respond %zu", sizeof(buffer));
                if (!flowFile) {
                  flowFile = session->create();
                  if (!flowFile)
                    continue;
                  flowFile->addAttribute("command", command_);
                  flowFile->addAttribute("command.arguments", command_argument_);
                  session->writeBuffer(flowFile, buffer);
                } else {
                  session->appendBuffer(flowFile, buffer);
                }
                // Rewind
                totalRead = 0;
                bufPtr = buffer;
              } else {
                totalRead += numRead;
                bufPtr += numRead;
              }
            }
          }
        }

        wait(&status);
        if (WIFEXITED(status)) {
          logger_->log_info("Execute Command Complete %s status %d pid %d", full_command_, WEXITSTATUS(status), pid_);
        } else {
          logger_->log_info("Execute Command Complete %s status %d pid %d", full_command_, WTERMSIG(status), pid_);
        }

        close(pipefd_[0]);
        process_running_ = false;
        break;
    }
  }
}

REGISTER_RESOURCE(ExecuteProcess, Processor);

#endif
}  // namespace org::apache::nifi::minifi::processors

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
