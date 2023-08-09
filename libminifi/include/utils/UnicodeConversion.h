/**
 * @file UnicodeConversion.h
 * Unicode conversion functions
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
