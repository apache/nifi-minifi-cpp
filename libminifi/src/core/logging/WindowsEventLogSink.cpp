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

#ifdef WIN32

#include "core/logging/WindowsEventLogSink.h"

#include <string>

#include "core/logging/WindowsMessageTextFile.h"
#include "minifi-cpp/Exception.h"
#include "spdlog/common.h"
#include "spdlog/sinks/sink.h"
#include "spdlog/details/log_msg.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

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

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif
