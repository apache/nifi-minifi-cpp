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

#pragma once

#include <Windows.h>
#include <winevt.h>

#include <string>

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::wel {

class EventPath {
 public:
  enum class Kind {
    CHANNEL,
    FILE
  };

  EventPath() = default;
  explicit EventPath(const std::wstring& wstr);
  explicit EventPath(std::string str);

  [[nodiscard]] constexpr const std::wstring& wstr() const noexcept {
    return wstr_;
  }

  [[nodiscard]] constexpr const std::string& str() const noexcept {
    return str_;
  }

  [[nodiscard]] constexpr EventPath::Kind kind() const noexcept {
    return kind_;
  }

  [[nodiscard]] constexpr EVT_QUERY_FLAGS getQueryFlags() const noexcept {
    switch (kind_) {
      case Kind::CHANNEL: return EvtQueryChannelPath;
      case Kind::FILE: return EvtQueryFilePath;
    }
    gsl_FailFast();
  }

 private:
  std::string str_;
  std::wstring wstr_;

  Kind kind_{Kind::CHANNEL};
};

}  // namespace org::apache::nifi::minifi::wel
