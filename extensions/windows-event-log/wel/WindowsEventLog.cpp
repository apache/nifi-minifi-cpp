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
#include <winmeta.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

#include "WindowsEventLog.h"
#include "UnicodeConversion.h"
#include "utils/Deleters.h"
#include "utils/gsl.h"
#include "UniqueEvtHandle.h"

namespace org::apache::nifi::minifi::wel {

namespace {
std::string getEventTimestampStr(uint64_t event_timestamp) {
  constexpr std::chrono::duration<int64_t> nt_to_unix_epoch{-11644473600};  // January 1, 1601 (NT epoch) - January 1, 1970 (Unix epoch):

  const std::chrono::duration<int64_t, std::ratio<1, 10'000'000>> event_timestamp_as_duration{event_timestamp};
  const auto converted_timestamp = std::chrono::system_clock::time_point{event_timestamp_as_duration + nt_to_unix_epoch};

  return date::format("%m/%d/%Y %r %p", std::chrono::floor<std::chrono::milliseconds>(converted_timestamp));
}
}  // namespace

void WindowsEventLogMetadataImpl::renderMetadata() {
  DWORD status = ERROR_SUCCESS;
  EVT_VARIANT stackBuffer[4096];
  DWORD dwBufferSize = sizeof(stackBuffer);
  using Deleter = utils::StackAwareDeleter<EVT_VARIANT, utils::FreeDeleter>;
  std::unique_ptr<EVT_VARIANT, Deleter> rendered_values{stackBuffer, Deleter{stackBuffer}};
  DWORD dwBufferUsed = 0;
  DWORD dwPropertyCount = 0;

  unique_evt_handle context{EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem)};
  if (!context)
    return;

  if (!EvtRender(context.get(), event_ptr_, EvtRenderEventValues, dwBufferSize, rendered_values.get(), &dwBufferUsed, &dwPropertyCount)) {
    if (ERROR_INSUFFICIENT_BUFFER != (status = GetLastError())) {
      return;
    }

    dwBufferSize = dwBufferUsed;
    rendered_values.reset((PEVT_VARIANT) (malloc(dwBufferSize)));
    if (!rendered_values) {
      return;
    }

    EvtRender(context.get(), event_ptr_, EvtRenderEventValues, dwBufferSize, rendered_values.get(), &dwBufferUsed, &dwPropertyCount);
    if (ERROR_SUCCESS != (status = GetLastError())) {
      return;
    }
  }

  event_timestamp_str_ = getEventTimestampStr(static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemTimeCreated].FileTimeVal);

  auto level = static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemLevel];
  auto keyword = static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemKeywords];
  if (level.Type == EvtVarTypeByte) {
    switch (level.ByteVal) {
      case WINEVENT_LEVEL_CRITICAL:
      case WINEVENT_LEVEL_ERROR:
        event_type_ = "Error";
        event_type_index_ = 1;
        break;
      case WINEVENT_LEVEL_WARNING:
        event_type_ = "Warning";
        event_type_index_ = 2;
        break;
      case WINEVENT_LEVEL_INFO:
      case WINEVENT_LEVEL_VERBOSE:
        event_type_ = "Information";
        event_type_index_ = 4;
        break;
      default:
        event_type_index_ = 0;
    }
  } else {
    event_type_ = "N/A";
  }

  if (keyword.UInt64Val & WINEVENT_KEYWORD_AUDIT_SUCCESS) {
    event_type_ = "Success Audit";
    event_type_index_ = 8;
  } else if (keyword.UInt64Val & EVENTLOG_AUDIT_FAILURE) {
    event_type_ = "Failure Audit";
    event_type_index_ = 16;
  }
}

std::string WindowsEventLogMetadataImpl::getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const {
  WCHAR stack_buffer[4096];
  DWORD num_chars_in_buffer = sizeof(stack_buffer) / sizeof(stack_buffer[0]);
  using Deleter = utils::StackAwareDeleter<WCHAR, utils::FreeDeleter>;
  std::unique_ptr<WCHAR, Deleter> buffer{stack_buffer, Deleter{stack_buffer}};
  DWORD num_chars_used = 0;

  std::string event_data;

  if (!metadata_ptr_ || !event_ptr_) {
    return event_data;
  }

  if (!EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, nullptr, flags, num_chars_in_buffer, buffer.get(), &num_chars_used)) {
    auto last_error = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == last_error) {
      num_chars_in_buffer = num_chars_used;

      buffer.reset((LPWSTR) malloc(num_chars_in_buffer * sizeof(WCHAR)));
      if (!buffer) {
        return event_data;
      }

      EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, nullptr, flags, num_chars_in_buffer, buffer.get(), &num_chars_used);
    }
  }

  if (num_chars_used == 0) {
    return event_data;
  }

  if (EvtFormatMessageKeyword == flags) {
    buffer.get()[num_chars_used - 1] = L'\0';
  }
  std::wstring str(buffer.get());
  event_data = std::string(str.begin(), str.end());
  return event_data;
}

nonstd::expected<std::string, std::error_code> WindowsEventLogHandler::getEventMessage(EVT_HANDLE eventHandle) const {
  std::string returnValue;
  WCHAR stack_buffer[4096];
  DWORD num_chars_in_buffer = sizeof(stack_buffer) / sizeof(stack_buffer[0]);
  using Deleter = utils::StackAwareDeleter<WCHAR, utils::FreeDeleter>;
  std::unique_ptr<WCHAR, Deleter> buffer{stack_buffer, Deleter{stack_buffer}};
  DWORD num_chars_used = 0;

  bool evt_format_succeeded = EvtFormatMessage(metadata_provider_.get(), eventHandle, 0, 0, nullptr, EvtFormatMessageEvent, num_chars_in_buffer, buffer.get(), &num_chars_used);
  if (evt_format_succeeded)
    return to_string(buffer.get());

  DWORD status = GetLastError();

  if (status != ERROR_INSUFFICIENT_BUFFER)
    return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(status));

  num_chars_in_buffer = num_chars_used;
  buffer.reset((LPWSTR) malloc(num_chars_in_buffer * sizeof(WCHAR)));
  if (!buffer)
    return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(ERROR_OUTOFMEMORY));
  if (EvtFormatMessage(metadata_provider_.get(), eventHandle, 0, 0, nullptr,
                       EvtFormatMessageEvent, num_chars_in_buffer,
                       buffer.get(), &num_chars_used))
    return to_string(buffer.get());
  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(GetLastError()));
}

namespace {
size_t findLongestHeaderNameSize(const METADATA_NAMES& header_names, const size_t minimum_size) {
  size_t max = minimum_size;
  for (const auto& option : header_names) {
    max = (std::max(max, option.second.size()));
  }
  return ++max;
}
}  // namespace

WindowsEventLogHeader::WindowsEventLogHeader(const METADATA_NAMES& header_names, const std::optional<std::string>& custom_delimiter, const size_t minimum_size)
    : header_names_(header_names),
      custom_delimiter_(custom_delimiter),
      longest_header_name_(findLongestHeaderNameSize(header_names, minimum_size)) {
}

std::string WindowsEventLogHeader::getDelimiterFor(size_t length) const {
  if (custom_delimiter_)
    return *custom_delimiter_;
  return createDefaultDelimiter(length);
}

std::string WindowsEventLogHeader::createDefaultDelimiter(size_t length) const {
  if (longest_header_name_ > length) {
    return ":" + std::string(longest_header_name_ - length, ' ');
  } else {
    return ": ";
  }
}

EVT_HANDLE WindowsEventLogHandler::getMetadata() const {
  return metadata_provider_.get();
}

}  // namespace org::apache::nifi::minifi::wel
