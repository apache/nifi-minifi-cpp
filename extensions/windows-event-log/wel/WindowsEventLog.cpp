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
#include "WindowsEventLog.h"

#include <winmeta.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

#include "utils/Deleters.h"
#include "utils/gsl.h"
#include "utils/UnicodeConversion.h"
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

nonstd::expected<std::string, std::error_code> formatEvent(EVT_HANDLE metadata, EVT_HANDLE event, EVT_FORMAT_MESSAGE_FLAGS flags) noexcept try {
  gsl_Expects(metadata && event);
  // first EvtFormatMessage call with no buffer to determine the required buffer size.
  DWORD buffer_size_in_number_of_WCHARs = 0;
  {
    const bool size_check_result = EvtFormatMessage(metadata, event, 0, 0, nullptr, flags, 0, nullptr, &buffer_size_in_number_of_WCHARs);
    if (size_check_result) {
      // format succeeded with no buffer, this has to be an empty result
      return std::string{};
    }
    const auto last_error = GetLastError();
    if (last_error != ERROR_INSUFFICIENT_BUFFER) {
      return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(last_error));
    }
  }

  // second EvtFormatMessage call: format and convert/narrow to std::string
  {
    static_assert(std::is_same_v<std::wstring, std::basic_string<WCHAR>>, "assuming that a string of WCHAR is wstring");
    DWORD out_buffer_size = buffer_size_in_number_of_WCHARs;
    std::wstring out_buffer;
    out_buffer.resize(out_buffer_size);
    const bool format_result = EvtFormatMessage(metadata, event, 0, 0, nullptr, flags, gsl::narrow<DWORD>(out_buffer.size()), out_buffer.data(), &out_buffer_size);
    if (!format_result) {
      const auto last_error = GetLastError();
      return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(last_error));
    }

    gsl_Assert(buffer_size_in_number_of_WCHARs == out_buffer_size && "message size shouldn't change between invocations of EvtFormatMessage");

    // fixing up keyword lists based on the example at https://learn.microsoft.com/en-us/windows/win32/wes/formatting-event-messages
    if (EvtFormatMessageKeyword == flags) {
      out_buffer[buffer_size_in_number_of_WCHARs - 1] = L'\0';
    }

    return utils::to_string(out_buffer);
  }
} catch (const std::bad_alloc&) {
  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(ERROR_OUTOFMEMORY));
}

nonstd::expected<std::string, std::error_code> WindowsEventLogMetadataImpl::getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const noexcept {
  if (!metadata_ptr_ || !event_ptr_) {
    // nothing to format, keep old behavior of returning an empty string
    return std::string{};
  }

  return formatEvent(metadata_ptr_, event_ptr_, flags);
}

nonstd::expected<std::string, std::error_code> WindowsEventLogHandler::getEventMessage(EVT_HANDLE eventHandle) const noexcept {
  return formatEvent(metadata_provider_.get(), eventHandle, EvtFormatMessageEvent);
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

}  // namespace org::apache::nifi::minifi::wel
