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

namespace org::apache::nifi::minifi::wel {

EventPath::EventPath(std::wstring wstr) : EventPath(std::string(wstr.begin(), wstr.end())) {}

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


constexpr const std::wstring& EventPath::wstr() const noexcept {
  return wstr_;
}

constexpr const std::string& EventPath::str() const noexcept {
  return str_;
}

constexpr EventPath::Kind EventPath::kind() const noexcept {
  return kind_;
}

constexpr EVT_QUERY_FLAGS EventPath::getQueryFlags() const noexcept {
  switch (kind_) {
    case Kind::CHANNEL: return EvtQueryChannelPath;
    case Kind::FILE: return EvtQueryFilePath;
  }
}

}  // namespace org::apache::nifi::minifi::wel
