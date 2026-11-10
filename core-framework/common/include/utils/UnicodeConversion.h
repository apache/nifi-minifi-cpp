// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <atlbase.h>
#include <atlconv.h>
#include <string>

namespace org::apache::nifi::minifi::utils {

inline std::string to_string(const std::wstring& utf16_string) {
  ATL::CW2A utf8_string(utf16_string.c_str(), CP_UTF8);
  return {LPSTR{utf8_string}};
}

inline std::wstring to_wstring(const std::string& utf8_string) {
  ATL::CA2W utf16_string(utf8_string.c_str(), CP_UTF8);
  return {LPWSTR{utf16_string}};
}

}  // namespace org::apache::nifi::minifi::utils
