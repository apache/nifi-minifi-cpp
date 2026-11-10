// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
