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

#include <Windows.h>

#include <system_error>

#include "fmt/format.h"

namespace org::apache::nifi::minifi::wel {

struct WindowsError {
  DWORD error_code;
};

inline WindowsError getLastError() {
  return {GetLastError()};
}

// this is called by fmt::format; see https://fmt.dev/11.1/api/#formatting-user-defined-types
inline std::string format_as(WindowsError windows_error) {
  std::error_code error_code(windows_error.error_code, std::system_category());
  return fmt::format("error 0x{:X}: {}", error_code.value(), error_code.message());
}

}  // namespace org::apache::nifi::minifi::wel
