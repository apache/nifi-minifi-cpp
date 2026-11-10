// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
