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
#include <set>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
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
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Command);
  properties.insert(CommandArguments);
  properties.insert(WorkingDir);
  properties.insert(BatchDuration);
  properties.insert(RedirectErrorStream);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ExecuteProcess::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::string value;
  std::shared_ptr<core::FlowFile> flow_file;
  if (context->getProperty(Command, value, flow_file)) {
    this->_command = value;
  }
  if (context->getProperty(CommandArguments, value, flow_file)) {
    this->_commandArgument = value;
  }
  if (context->getProperty(WorkingDir, value, flow_file)) {
    this->_workingDir = value;
  }
  if (context->getProperty(BatchDuration.getName(), value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, _batchDuration, unit) && core::Property::ConvertTimeUnitToMS(_batchDuration, unit, _batchDuration)) {
      logger_->log_debug("Setting _batchDuration");
    }
  }
  if (context->getProperty(RedirectErrorStream.getName(), value)) {
    _redirectErrorStream =  org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }
  this->_fullCommand = _command + " " + _commandArgument;
  if (_fullCommand.length() == 0) {
    yield();
    return;
  }
  if (_workingDir.length() > 0 && _workingDir != ".") {
    // change to working directory
    if (chdir(_workingDir.c_str()) != 0) {
      logger_->log_error("Execute Command can not chdir %s", _workingDir);
      yield();
      return;
    }
  }
  logger_->log_info("Execute Command %s", _fullCommand);
  // split the command into array
  char *p = std::strtok(const_cast<char*>(_fullCommand.c_str()), " ");
  int argc = 0;
  char *argv[64];
  while (p != 0 && argc < 64) {
    argv[argc] = p;
    p = std::strtok(NULL, " ");
    argc++;
  }
  argv[argc] = NULL;
  int status;
  if (!_processRunning) {
    _processRunning = true;
    // if the process has not launched yet
    // create the pipe
    if (pipe(_pipefd) == -1) {
      _processRunning = false;
      yield();
      return;
    }
    switch (_pid = fork()) {
      case -1:
        logger_->log_error("Execute Process fork failed");
        _processRunning = false;
        close(_pipefd[0]);
        close(_pipefd[1]);
        yield();
        break;
      case 0:  // this is the code the child runs
        close(1);      // close stdout
        dup(_pipefd[1]);  // points pipefd at file descriptor
        if (_redirectErrorStream)
          // redirect stderr
          dup2(_pipefd[1], 2);
        close(_pipefd[0]);
        execvp(argv[0], argv);
        exit(1);
        break;
      default:  // this is the code the parent runs
        // the parent isn't going to write to the pipe
        close(_pipefd[1]);
        if (_batchDuration > 0) {
          while (1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(_batchDuration));
            char buffer[4096];
            const auto  numRead = read(_pipefd[0], buffer, sizeof(buffer));
            if (numRead <= 0)
              break;
            logger_->log_debug("Execute Command Respond %zd", numRead);
            ExecuteProcess::WriteCallback callback(buffer, gsl::narrow<uint64_t>(numRead));
            auto flowFile = session->create();
            if (!flowFile)
              continue;
            flowFile->addAttribute("command", _command);
            flowFile->addAttribute("command.arguments", _commandArgument);
            session->write(flowFile, &callback);
            session->transfer(flowFile, Success);
            session->commit();
          }
        } else {
          char buffer[4096];
          char *bufPtr = buffer;
          size_t totalRead = 0;
          std::shared_ptr<core::FlowFile> flowFile = nullptr;
          while (true) {
            const auto numRead = read(_pipefd[0], bufPtr, (sizeof(buffer) - totalRead));
            if (numRead <= 0) {
              if (totalRead > 0) {
                logger_->log_debug("Execute Command Respond %zu", totalRead);
                // child exits and close the pipe
                ExecuteProcess::WriteCallback callback(buffer, totalRead);
                if (!flowFile) {
                  flowFile = session->create();
                  if (!flowFile)
                    break;
                  flowFile->addAttribute("command", _command);
                  flowFile->addAttribute("command.arguments", _commandArgument);
                  session->write(flowFile, &callback);
                } else {
                  session->append(flowFile, &callback);
                }
                session->transfer(flowFile, Success);
              }
              break;
            } else {
              if (numRead == static_cast<ssize_t>((sizeof(buffer) - totalRead))) {
                // we reach the max buffer size
                logger_->log_debug("Execute Command Max Respond %zu", sizeof(buffer));
                ExecuteProcess::WriteCallback callback(buffer, sizeof(buffer));
                if (!flowFile) {
                  flowFile = session->create();
                  if (!flowFile)
                    continue;
                  flowFile->addAttribute("command", _command);
                  flowFile->addAttribute("command.arguments", _commandArgument);
                  session->write(flowFile, &callback);
                } else {
                  session->append(flowFile, &callback);
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
          logger_->log_info("Execute Command Complete %s status %d pid %d", _fullCommand, WEXITSTATUS(status), _pid);
        } else {
          logger_->log_info("Execute Command Complete %s status %d pid %d", _fullCommand, WTERMSIG(status), _pid);
        }

        close(_pipefd[0]);
        _processRunning = false;
        break;
    }
  }
}

REGISTER_RESOURCE(ExecuteProcess, "Runs an operating system command specified by the user and writes the output of that command to a FlowFile. If the command is expected to be long-running,"
                  "the Processor can output the partial data on a specified interval. When this option is used, the output is expected to be in textual format,"
                  "as it typically does not make sense to split binary data on arbitrary time-based intervals.");

#endif
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
