/**
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

#include "EventPath.h"
#include <utility>
#include "utils/StringUtils.h"
#include "utils/UnicodeConversion.h"

namespace org::apache::nifi::minifi::wel {

EventPath::EventPath(const std::wstring& wstr) : EventPath(utils::to_string(wstr)) {}

EventPath::EventPath(std::string str) {
  constexpr std::string_view saved_log_prefix = "SavedLog:";
  if (utils::StringUtils::startsWith(str, saved_log_prefix)) {
    str_ = str.substr(saved_log_prefix.size());
    kind_ = Kind::FILE;
  } else {
    str_ = std::move(str);
    kind_ = Kind::CHANNEL;
  }
  wstr_ = std::wstring(str_.begin(), str_.end());
}

}  // namespace org::apache::nifi::minifi::wel
