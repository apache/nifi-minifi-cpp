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
#include "processors/ExecuteProcess.h"
#include <cstring>
#include <memory>
#include <string>
#include <set>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ExecuteProcess::Command("Command", "Specifies the command to be executed; if just the name of an executable"
                                       " is provided, it must be in the user's environment PATH.",
                                       "");
core::Property ExecuteProcess::CommandArguments("Command Arguments", "The arguments to supply to the executable delimited by white space. White "
                                                "space can be escaped by enclosing it in double-quotes.",
                                                "");
core::Property ExecuteProcess::WorkingDir("Working Directory", "The directory to use as the current working directory when executing the command", "");
core::Property ExecuteProcess::BatchDuration("Batch Duration", "If the process is expected to be long-running and produce textual output, a "
                                             "batch duration can be specified.",
                                             "0");
core::Property ExecuteProcess::RedirectErrorStream("Redirect Error Stream", "If true will redirect any error stream output of the process to the output stream.", "false");
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
  if (context->getProperty(Command.getName(), value)) {
    this->_command = value;
  }
  if (context->getProperty(CommandArguments.getName(), value)) {
    this->_commandArgument = value;
  }
  if (context->getProperty(WorkingDir.getName(), value)) {
    this->_workingDir = value;
  }
  if (context->getProperty(BatchDuration.getName(), value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, _batchDuration, unit) && core::Property::ConvertTimeUnitToMS(_batchDuration, unit, _batchDuration)) {
      logger_->log_info("Setting _batchDuration");
    }
  }
  if (context->getProperty(RedirectErrorStream.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, _redirectErrorStream);
  }
  this->_fullCommand = _command + " " + _commandArgument;
  if (_fullCommand.length() == 0) {
    yield();
    return;
  }
  if (_workingDir.length() > 0 && _workingDir != ".") {
    // change to working directory
    if (chdir(_workingDir.c_str()) != 0) {
      logger_->log_error("Execute Command can not chdir %s", _workingDir.c_str());
      yield();
      return;
    }
  }
  logger_->log_info("Execute Command %s", _fullCommand.c_str());
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
  int status, died;
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
            int numRead = read(_pipefd[0], buffer, sizeof(buffer));
            if (numRead <= 0)
              break;
            logger_->log_info("Execute Command Respond %d", numRead);
            ExecuteProcess::WriteCallback callback(buffer, numRead);
            std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
            if (!flowFile)
              continue;
            flowFile->addAttribute("command", _command.c_str());
            flowFile->addAttribute("command.arguments", _commandArgument.c_str());
            session->write(flowFile, &callback);
            session->transfer(flowFile, Success);
            session->commit();
          }
        } else {
          char buffer[4096];
          char *bufPtr = buffer;
          int totalRead = 0;
          std::shared_ptr<FlowFileRecord> flowFile = nullptr;
          while (1) {
            int numRead = read(_pipefd[0], bufPtr, (sizeof(buffer) - totalRead));
            if (numRead <= 0) {
              if (totalRead > 0) {
                logger_->log_info("Execute Command Respond %d", totalRead);
                // child exits and close the pipe
                ExecuteProcess::WriteCallback callback(buffer, totalRead);
                if (!flowFile) {
                  flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
                  if (!flowFile)
                    break;
                  flowFile->addAttribute("command", _command.c_str());
                  flowFile->addAttribute("command.arguments", _commandArgument.c_str());
                  session->write(flowFile, &callback);
                } else {
                  session->append(flowFile, &callback);
                }
                session->transfer(flowFile, Success);
              }
              break;
            } else {
              if (numRead == (sizeof(buffer) - totalRead)) {
                // we reach the max buffer size
                logger_->log_info("Execute Command Max Respond %d", sizeof(buffer));
                ExecuteProcess::WriteCallback callback(buffer, sizeof(buffer));
                if (!flowFile) {
                  flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
                  if (!flowFile)
                    continue;
                  flowFile->addAttribute("command", _command.c_str());
                  flowFile->addAttribute("command.arguments", _commandArgument.c_str());
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

        died = wait(&status);
        if (WIFEXITED(status)) {
          logger_->log_info("Execute Command Complete %s status %d pid %d", _fullCommand.c_str(), WEXITSTATUS(status), _pid);
        } else {
          logger_->log_info("Execute Command Complete %s status %d pid %d", _fullCommand.c_str(), WTERMSIG(status), _pid);
        }

        close(_pipefd[0]);
        _processRunning = false;
        break;
    }
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
