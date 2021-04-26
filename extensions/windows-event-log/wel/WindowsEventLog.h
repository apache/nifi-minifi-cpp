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

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include <pugixml.hpp>
#include <winevt.h>
#include <sstream>
#include <regex>
#include <codecvt>
#include "utils/OsUtils.h"



namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {


  enum METADATA {
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


// this is a continuous enum so we can rely on the array

typedef std::vector<std::pair<METADATA, std::string>> METADATA_NAMES;

class WindowsEventLogHandler {
public:
  WindowsEventLogHandler() : metadata_provider_(nullptr) {
  }

  explicit WindowsEventLogHandler(EVT_HANDLE metadataProvider) : metadata_provider_(metadataProvider) {
  }

  std::string getEventMessage(EVT_HANDLE eventHandle) const;


  EVT_HANDLE getMetadata() const;

private:
  EVT_HANDLE metadata_provider_;
};

class WindowsEventLogMetadata {
 public:
  virtual ~WindowsEventLogMetadata() = default;
  virtual std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const = 0;
  virtual std::string getEventTimestamp() const = 0;
  virtual short getEventTypeIndex() const = 0;

  static std::string getMetadataString(METADATA val) {
    static std::map< METADATA, std::string> map = {
        {LOG_NAME,  "LOG_NAME" },
        {SOURCE, "SOURCE"},
        {TIME_CREATED, "TIME_CREATED" },
        {EVENTID, "EVENTID"},
        {OPCODE, "OPCODE"},
        {EVENT_RECORDID, "EVENT_RECORDID"},
        {EVENT_TYPE, "EVENT_TYPE"},
        {TASK_CATEGORY, "TASK_CATEGORY"},
        {LEVEL, "LEVEL"},
        {KEYWORDS, "KEYWORDS"},
        {USER, "USER"},
        {COMPUTER, "COMPUTER"}
    };

    return map[val];
  }

  static METADATA getMetadataFromString(const std::string &val) {
    static std::map< std::string, METADATA> map = {
        {"LOG_NAME", LOG_NAME},
        {"SOURCE", SOURCE},
        {"TIME_CREATED", TIME_CREATED },
        {"EVENTID", EVENTID},
        {"OPCODE", OPCODE},
        {"EVENT_RECORDID", EVENT_RECORDID},
        {"TASK_CATEGORY", TASK_CATEGORY},
        {"EVENT_TYPE", EVENT_TYPE},
        {"LEVEL", LEVEL},
        {"KEYWORDS", KEYWORDS},
        {"USER", USER},
        {"COMPUTER", COMPUTER}
    };

    auto enumVal = map.find(val);
    if (enumVal != std::end(map)) {
      return enumVal->second;
    }
    else {
      return METADATA::UNKNOWN;
    }
  }

  static std::string getComputerName() {
    static std::string computer_name;
    if (computer_name.empty()) {
      char buff[10248];
      DWORD size = sizeof(buff);
      if (GetComputerNameExA(ComputerNameDnsFullyQualified, buff, &size)) {
        computer_name = buff;
      }
      else {
        computer_name = "N/A";
      }
    }
    return computer_name;
  }
};

class WindowsEventLogMetadataImpl : public WindowsEventLogMetadata {
public:
  WindowsEventLogMetadataImpl(EVT_HANDLE metadataProvider, EVT_HANDLE event_ptr) : metadata_ptr_(metadataProvider), event_timestamp_(0), event_ptr_(event_ptr) {
    renderMetadata();
  }

  std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const override;

  std::string getEventTimestamp() const override { return event_timestamp_str_; }

  short getEventTypeIndex() const override { return event_type_index_; }

 private:
  void renderMetadata();

  uint64_t event_timestamp_;
  std::string event_type_;
  short event_type_index_;
  std::string event_timestamp_str_;
  EVT_HANDLE event_ptr_;
  EVT_HANDLE metadata_ptr_;
};

class WindowsEventLogHeader {
public:
  explicit WindowsEventLogHeader(METADATA_NAMES header_names) : header_names_(header_names) {}

  void setDelimiter(const std::string &delim);

  template<typename MetadataCollection>
  std::string getEventHeader(const MetadataCollection& metadata_collection) const;

private:
  std::string createDefaultDelimiter(size_t max, size_t length) const;

  std::string delimiter_;
  METADATA_NAMES header_names_;
};

template<typename MetadataCollection>
std::string WindowsEventLogHeader::getEventHeader(const MetadataCollection& metadata_collection) const {
  std::stringstream eventHeader;
  size_t max = 1;
  for (const auto &option : header_names_) {
    max = (std::max(max, option.second.size()));
  }
  ++max;  // increment by one to get space.
  for (const auto &option : header_names_) {
    auto name = option.second;
    if (!name.empty()) {
      eventHeader << name << (delimiter_.empty() ? createDefaultDelimiter(max, name.size()) : delimiter_);
    }
    eventHeader << utils::StringUtils::trim(metadata_collection(option.first)) << std::endl;
  }

  return eventHeader.str();
}

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

