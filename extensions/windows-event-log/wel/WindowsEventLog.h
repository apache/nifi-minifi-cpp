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
#include "concurrentqueue.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "utils/OsUtils.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "UniqueEvtHandle.h"

#include "pugixml.hpp"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::wel {

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
};

class EventDataCache {
 public:
  explicit EventDataCache(std::chrono::milliseconds lifetime = std::chrono::hours{24})
      : lifetime_(lifetime) {}
  [[nodiscard]] std::optional<std::string> get(EVT_FORMAT_MESSAGE_FLAGS field, const std::string& key) const;
  void set(EVT_FORMAT_MESSAGE_FLAGS field, const std::string& key, std::string value);

 private:
  struct CacheKey {
    EVT_FORMAT_MESSAGE_FLAGS field;
    std::string key;

    [[nodiscard]] bool operator==(const CacheKey&) const noexcept = default;
  };
  struct CacheKeyHash {
    [[nodiscard]] size_t operator()(const CacheKey& cache_key) const noexcept {
      return utils::hash_combine(std::hash<EVT_FORMAT_MESSAGE_FLAGS>{}(cache_key.field), std::hash<std::string>{}(cache_key.key));
    }
  };
  struct CacheItem {
    std::string value;
    std::chrono::system_clock::time_point expiry;
  };

  mutable std::mutex mutex_;
  std::chrono::milliseconds lifetime_;
  std::unordered_map<CacheKey, CacheItem, CacheKeyHash> cache_;
};

class WindowsEventLogProvider {
 public:
  WindowsEventLogProvider() : provider_handle_(nullptr) {}
  explicit WindowsEventLogProvider(EVT_HANDLE provider_handle) : provider_handle_(provider_handle) {}

  nonstd::expected<std::string, std::error_code> getEventMessage(EVT_HANDLE event_handle) const;
  [[nodiscard]] std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS field, const std::string& key, EVT_HANDLE event_handle) const;

 private:
  [[nodiscard]] std::string getEventDataImpl(EVT_FORMAT_MESSAGE_FLAGS field, EVT_HANDLE event_handle) const;

  unique_evt_handle provider_handle_;
  mutable EventDataCache event_data_cache_;
};

class WindowsEventLogMetadata {
 public:
  virtual ~WindowsEventLogMetadata() = default;
  [[nodiscard]] virtual std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS field, const std::string& key) const = 0;
  [[nodiscard]] virtual std::string getEventTimestamp() const = 0;
  virtual short getEventTypeIndex() const = 0;  // NOLINT short comes from WINDOWS API

  static std::string getComputerName() {
    static std::string computer_name;
    if (computer_name.empty()) {
      char buff[10248];
      DWORD size = sizeof(buff);
      if (GetComputerNameExA(ComputerNameDnsFullyQualified, buff, &size)) {
        computer_name = buff;
      } else {
        computer_name = "N/A";
      }
    }
    return computer_name;
  }
};

class WindowsEventLogMetadataImpl : public WindowsEventLogMetadata {
 public:
  WindowsEventLogMetadataImpl(const WindowsEventLogProvider& event_log_provider, EVT_HANDLE event_handle) : event_log_provider_(event_log_provider), event_handle_(event_handle) {
    renderMetadata();
  }

  [[nodiscard]] std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS field, const std::string& key) const override;

  [[nodiscard]] std::string getEventTimestamp() const override { return event_timestamp_str_; }

  short getEventTypeIndex() const override { return event_type_index_; }  // NOLINT short comes from WINDOWS API

 private:
  void renderMetadata();

  std::string event_type_;
  short event_type_index_ = 0;  // NOLINT short comes from WINDOWS API
  std::string event_timestamp_str_;
  EVT_HANDLE event_handle_;
  const WindowsEventLogProvider& event_log_provider_;
};

using HeaderNames = std::vector<std::pair<Metadata, std::string>>;

class WindowsEventLogHeader {
 public:
  explicit WindowsEventLogHeader(const HeaderNames& header_names, const std::optional<std::string>& custom_delimiter, size_t minimum_size);

  template<typename MetadataCollection>
  std::string getEventHeader(const MetadataCollection& metadata_collection) const;

  [[nodiscard]] std::string getDelimiterFor(size_t length) const;
 private:
  [[nodiscard]] std::string createDefaultDelimiter(size_t length) const;

  const HeaderNames& header_names_;
  const std::optional<std::string>& custom_delimiter_;
  const size_t longest_header_name_;
};

template<typename MetadataCollection>
std::string WindowsEventLogHeader::getEventHeader(const MetadataCollection& metadata_collection) const {
  std::stringstream eventHeader;

  for (const auto& option : header_names_) {
    auto name = option.second;
    eventHeader << name;
    if (custom_delimiter_)
      eventHeader << *custom_delimiter_;
    else
      eventHeader << createDefaultDelimiter(name.size());
    eventHeader << utils::string::trim(metadata_collection(option.first)) << std::endl;
  }

  return eventHeader.str();
}

}  // namespace org::apache::nifi::minifi::wel
