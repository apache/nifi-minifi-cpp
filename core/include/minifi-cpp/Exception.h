/**
 * @file Exception.h
 * Exception class declaration
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

#include <errno.h>
#include <string.h>

#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi {

enum ExceptionType {
  FILE_OPERATION_EXCEPTION = 0,
  FLOW_EXCEPTION,
  PROCESSOR_EXCEPTION,
  PROCESS_SESSION_EXCEPTION,
  PROCESS_SCHEDULE_EXCEPTION,
  SITE2SITE_EXCEPTION,
  GENERAL_EXCEPTION,
  REGEX_EXCEPTION,
  REPOSITORY_EXCEPTION,
  PARAMETER_EXCEPTION,
  MAX_EXCEPTION
};

static const char *ExceptionStr[MAX_EXCEPTION] = { "File Operation", "Flow File Operation", "Processor Operation", "Process Session Operation", "Process Schedule Operation", "Site2Site Protocol",
    "General Operation", "Regex Operation", "Repository Operation", "Parameter Operation"};

inline const char *ExceptionTypeToString(ExceptionType type) {
  if (type < MAX_EXCEPTION)
    return ExceptionStr[type];
  else
    return nullptr;
}

std::string getCurrentExceptionTypeName();

struct Exception : public std::runtime_error {
  /*!
   * Create a new exception
   */
  Exception(ExceptionType type, const std::string& errorMsg)
      :Exception{ utils::string::join_pack(ExceptionTypeToString(type), ": ", errorMsg) }
  { }

  Exception(ExceptionType type, const char* errorMsg)
      :Exception{ utils::string::join_pack(ExceptionTypeToString(type), ": ", errorMsg) }
  { }

 protected:
  explicit Exception(const std::string& errmsg)
      :std::runtime_error{ errmsg }
  {}
  explicit Exception(const char* errmsg)
      :std::runtime_error{ errmsg }
  {}
};

struct SystemErrorException : Exception {
  explicit SystemErrorException(const char* const operation, std::error_condition error_condition)
      :Exception{ utils::string::join_pack(operation, ": ", error_condition.message()) },
      error_condition_{ error_condition }
  {}

  std::error_condition error_condition() { return error_condition_; }

 private:
  std::error_condition error_condition_;
};

}  // namespace org::apache::nifi::minifi
