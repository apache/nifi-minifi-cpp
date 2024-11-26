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

#include <algorithm>
#include <codecvt>
#include <map>
#include <sstream>
#include <string>
#include <regex>
#include <utility>
#include <vector>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/OsUtils.h"
#include "FlowFileRecord.h"
#include "UniqueEvtHandle.h"

#include "pugixml.hpp"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::wel {

// enum names are part of the API
enum class Metadata {
  LOG_NAME,
  SOURCE,
  TIME_CREATED,
  EVENTID,
  OPCODE,
  EVENT_RECORDID,
  EVENT_TYPE,
  TASK_CATEGORY,
  LEVEL,
  KEYWORDS,
  USER,
  COMPUTER,
  UNKNOWN
};

using METADATA_NAMES = std::vector<std::pair<Metadata, std::string>>;

nonstd::expected<std::string, std::error_code> formatEvent(EVT_HANDLE metadata, EVT_HANDLE event, EVT_FORMAT_MESSAGE_FLAGS flags) noexcept;

class WindowsEventLogHandler {
 public:
  WindowsEventLogHandler() = default;
  explicit WindowsEventLogHandler(EVT_HANDLE metadataProvider)
      : metadata_provider_(metadataProvider)
  { }

  [[nodiscard]] nonstd::expected<std::string, std::error_code> getEventMessage(EVT_HANDLE eventHandle) const noexcept;
  [[nodiscard]] EVT_HANDLE getMetadata() const noexcept { return metadata_provider_.get(); }

 private:
  unique_evt_handle metadata_provider_;
};

class WindowsEventLogMetadata {
 public:
  virtual ~WindowsEventLogMetadata() = default;
  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const noexcept = 0;
  [[nodiscard]] virtual std::string getEventTimestamp() const = 0;
  virtual short getEventTypeIndex() const = 0;  // NOLINT short comes from WINDOWS API

  static Metadata getMetadataFromString(std::string_view val) {
    using enum Metadata;
    if (val == "LOG_NAME") { return LOG_NAME; }
    else if (val == "SOURCE") { return SOURCE; }
    else if (val == "TIME_CREATED") { return TIME_CREATED; }
    else if (val == "EVENTID") { return EVENTID; }
    else if (val == "OPCODE") { return OPCODE; }
    else if (val == "EVENT_RECORDID") { return EVENT_RECORDID; }
    else if (val == "EVENT_TYPE") { return EVENT_TYPE; }
    else if (val == "TASK_CATEGORY") { return TASK_CATEGORY; }
    else if (val == "LEVEL") { return LEVEL; }
    else if (val == "KEYWORDS") { return KEYWORDS; }
    else if (val == "USER") { return USER; }
    else if (val == "COMPUTER") { return COMPUTER; }
    else { return UNKNOWN; }
  }

  static std::string getComputerName() {
    static std::string computer_name = []() -> std::string {
      char buff[256] = {0};
      DWORD size = sizeof(buff);
      if (GetComputerNameExA(ComputerNameDnsFullyQualified, buff, &size) || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        buff[255] = '\0';  // It's not clear from the MSDN docs whether the terminating null byte is written
        return buff;
      } else {
        return "N/A";
      }
    }();
    return computer_name;
  }
};

class WindowsEventLogMetadataImpl : public WindowsEventLogMetadata {
 public:
  WindowsEventLogMetadataImpl(EVT_HANDLE metadataProvider, EVT_HANDLE event_ptr)
      : metadata_ptr_(metadataProvider), event_ptr_(event_ptr) {
    renderMetadata();
  }

  [[nodiscard]] nonstd::expected<std::string, std::error_code> getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const noexcept override;
  [[nodiscard]] std::string getEventTimestamp() const override { return event_timestamp_str_; }

  short getEventTypeIndex() const override { return event_type_index_; }  // NOLINT short comes from WINDOWS API

 private:
  void renderMetadata();

  std::string event_type_;
  short event_type_index_ = 0;  // NOLINT short comes from WINDOWS API
  std::string event_timestamp_str_;
  EVT_HANDLE event_ptr_;
  EVT_HANDLE metadata_ptr_;
};

class WindowsEventLogHeader {
 public:
  explicit WindowsEventLogHeader(const METADATA_NAMES& header_names, const std::optional<std::string>& custom_delimiter, size_t minimum_size);

  template<typename MetadataCollection>
  std::string getEventHeader(const MetadataCollection& metadata_collection) const;

  [[nodiscard]] std::string getDelimiterFor(size_t length) const;
 private:
  [[nodiscard]] std::string createDefaultDelimiter(size_t length) const;

  const METADATA_NAMES& header_names_;
  const std::optional<std::string>& custom_delimiter_;
  const size_t longest_header_name_;
};

template<typename MetadataCollection>
std::string WindowsEventLogHeader::getEventHeader(const MetadataCollection& metadata_collection) const {
  std::stringstream eventHeader;
  for (const auto& option : header_names_) {
    const auto& name = option.second;
    eventHeader << name;
    if (custom_delimiter_) {
      eventHeader << *custom_delimiter_;
    } else {
      eventHeader << createDefaultDelimiter(name.size());
    }
    eventHeader << utils::string::trim(metadata_collection(option.first)) << '\n';
  }
  return eventHeader.str();
}

}  // namespace org::apache::nifi::minifi::wel
