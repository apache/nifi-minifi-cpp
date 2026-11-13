// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  ASSET_EXCEPTION,
  MAX_EXCEPTION
};

static const char *ExceptionStr[MAX_EXCEPTION] = { "File Operation", "Flow File Operation", "Processor Operation", "Process Session Operation", "Process Schedule Operation", "Site2Site Protocol",
    "General Operation", "Regex Operation", "Repository Operation", "Parameter Operation", "Asset Operation" };

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
