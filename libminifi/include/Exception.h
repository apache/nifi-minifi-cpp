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
#ifndef LIBMINIFI_INCLUDE_EXCEPTION_H_
#define LIBMINIFI_INCLUDE_EXCEPTION_H_

#include <errno.h>
#include <string.h>

#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

enum ExceptionType {
  FILE_OPERATION_EXCEPTION = 0,
  FLOW_EXCEPTION,
  PROCESSOR_EXCEPTION,
  PROCESS_SESSION_EXCEPTION,
  PROCESS_SCHEDULE_EXCEPTION,
  SITE2SITE_EXCEPTION,
  GENERAL_EXCEPTION,
  REGEX_EXCEPTION,
  MAX_EXCEPTION
};

static const char *ExceptionStr[MAX_EXCEPTION] = { "File Operation", "Flow File Operation", "Processor Operation", "Process Session Operation", "Process Schedule Operation", "Site2Site Protocol",
    "General Operation", "Regex Operation" };

inline const char *ExceptionTypeToString(ExceptionType type) {
  if (type < MAX_EXCEPTION)
    return ExceptionStr[type];
  else
    return NULL;
}

struct Exception : public std::runtime_error {
  /*!
   * Create a new exception
   */
  Exception(ExceptionType type, const std::string& errorMsg)
      : std::runtime_error{ org::apache::nifi::minifi::utils::StringUtils::join_pack(ExceptionTypeToString(type), ": ", errorMsg) }
  { }

  Exception(ExceptionType type, const char* errorMsg)
      : std::runtime_error{ org::apache::nifi::minifi::utils::StringUtils::join_pack(ExceptionTypeToString(type), ": ", errorMsg) }
  { }
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_EXCEPTION_H_
