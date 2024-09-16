/**

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

#include <cstdint>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace annotation {

enum class Input : uint8_t {
  /**
   * This value is used to indicate that the Processor requires input from other Processors
   * in order to run. As a result, the Processor will not be valid if it does not have any
   * incoming connections.
   */
  INPUT_REQUIRED,
  /**
   * This value is used to indicate that the Processor will consume data from an incoming
   * connection but does not require an incoming connection in order to perform its task.
   * If no input annotation is present, this is the default value that is used.
   */
  INPUT_ALLOWED,
  /**
   * This value is used to indicate that the Processor is a "Source Processor" and does
   * not accept incoming connections. Because the Processor does not pull FlowFiles from
   * an incoming connection, it can be very confusing for users who create incoming connections
   * to the Processor. As a result, this structure can be used in order to clarify that incoming
   * connections will not be used. This prevents the user from even creating such a connection.
   */
  INPUT_FORBIDDEN
};

inline std::string toString(Input input_requirement) {
  switch (input_requirement) {
    case annotation::Input::INPUT_REQUIRED:
      return "INPUT_REQUIRED";
    case annotation::Input::INPUT_ALLOWED:
      return "INPUT_ALLOWED";
    case annotation::Input::INPUT_FORBIDDEN:
      return "INPUT_FORBIDDEN";
  }

  return "ERROR_no_such_input_requirement";
}

}  // namespace annotation
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
