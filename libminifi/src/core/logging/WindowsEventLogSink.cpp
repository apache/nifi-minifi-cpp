// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#ifdef WIN32

#include "core/logging/WindowsEventLogSink.h"

#include <string>

#include "core/logging/WindowsMessageTextFile.h"
#include "minifi-cpp/Exception.h"
#include "spdlog/common.h"
#include "spdlog/sinks/sink.h"
#include "spdlog/details/log_msg.h"

namespace org::apache::nifi::minifi::core::logging::internal {

WORD windowseventlog_sink::type_from_level(const spdlog::details::log_msg& msg) const {
  switch (static_cast<int>(msg.level)) {
    case spdlog::level::trace:
    case spdlog::level::debug:
    case spdlog::level::info:
      return EVENTLOG_INFORMATION_TYPE;
    case spdlog::level::warn:
      return EVENTLOG_WARNING_TYPE;
    case spdlog::level::err:
    case spdlog::level::critical:
      return EVENTLOG_ERROR_TYPE;
    default:
      return EVENTLOG_ERROR_TYPE;
  }
}

void windowseventlog_sink::sink_it_(const spdlog::details::log_msg& msg) {
  const char* formatted_msg = msg.payload.data();
  ReportEventA(event_source_,
               type_from_level(msg) /*wType*/,
               0U /*wCategory*/,
               MSG_DEFAULT /*dwEventID*/,
               nullptr /* lpUserSid */,
               1U /*wNumStrings*/,
               0U /*dwDataSize*/,
               &formatted_msg /*lpStrings*/,
               nullptr /*lpRawData*/);
}

void windowseventlog_sink::flush_() {
}

windowseventlog_sink::windowseventlog_sink(const std::string& source_name /*= "ApacheNiFiMiNiFi"*/)
: event_source_(nullptr) {
  event_source_ = RegisterEventSourceA(nullptr, source_name.c_str());
  if (event_source_ == nullptr) {
    throw Exception(GENERAL_EXCEPTION, "Failed to create event source");
  }
}

windowseventlog_sink::~windowseventlog_sink() {
  if (event_source_ != nullptr) {
    DeregisterEventSource(event_source_);
  }
}

}  // namespace org::apache::nifi::minifi::core::logging::internal

#endif
