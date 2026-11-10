// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "EventPath.h"
#include <utility>
#include "utils/StringUtils.h"
#include "utils/UnicodeConversion.h"

namespace org::apache::nifi::minifi::wel {

EventPath::EventPath(const std::wstring& wstr) : EventPath(utils::to_string(wstr)) {}

EventPath::EventPath(std::string str) {
  constexpr std::string_view saved_log_prefix = "SavedLog:";
  if (utils::string::startsWith(str, saved_log_prefix)) {
    str_ = str.substr(saved_log_prefix.size());
    kind_ = Kind::FILE;
  } else {
    str_ = std::move(str);
    kind_ = Kind::CHANNEL;
  }
  wstr_ = std::wstring(str_.begin(), str_.end());
}

}  // namespace org::apache::nifi::minifi::wel
