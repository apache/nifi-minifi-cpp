/**
 * @file ScriptException.h
 * ScriptException class declaration
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
#ifndef __SCRIPT_EXCEPTION_H__
#define __SCRIPT_EXCEPTION_H__

#include <sstream>
#include <exception>
#include <stdexcept>
#include <errno.h>
#include <string.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace script {
/**
 * Defines an exception that may occur within the confines of a script.
 */
class ScriptException : public std::exception {
 public:
  // Constructor
  /*!
   * Create a new exception
   */
  ScriptException(std::string errorMsg)
      : error_(std::move(errorMsg)) {
  }

  virtual ~ScriptException() noexcept {
  }

  virtual const char * what() const noexcept {
    return error_.c_str();
  }

 private:
  std::string error_;
};
} /* namespace script */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
