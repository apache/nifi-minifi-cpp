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
#include <memory>
#include <string>

#include "WindowsEventLog.h"
#include "UnicodeConversion.h"
#include "utils/Deleters.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::wel {

void WindowsEventLogMetadataImpl::renderMetadata() {
  DWORD status = ERROR_SUCCESS;
  EVT_VARIANT stackBuffer[4096];
  DWORD dwBufferSize = sizeof(stackBuffer);
  using Deleter = utils::StackAwareDeleter<EVT_VARIANT, utils::FreeDeleter>;
  std::unique_ptr<EVT_VARIANT, Deleter> rendered_values{ stackBuffer, Deleter{stackBuffer} };
  DWORD dwBufferUsed = 0;
  DWORD dwPropertyCount = 0;

  auto context = EvtCreateRenderContext(0, NULL, EvtRenderContextSystem);
  if (context == NULL) {
    return;
  }
  const auto contextGuard = gsl::finally([&context](){
    EvtClose(context);
  });
  if (!EvtRender(context, event_ptr_, EvtRenderEventValues, dwBufferSize, rendered_values.get(), &dwBufferUsed, &dwPropertyCount)) {
    if (ERROR_INSUFFICIENT_BUFFER != (status = GetLastError())) {
      return;
    }

    dwBufferSize = dwBufferUsed;
    rendered_values.reset((PEVT_VARIANT)(malloc(dwBufferSize)));
    if (!rendered_values) {
      return;
    }

    EvtRender(context, event_ptr_, EvtRenderEventValues, dwBufferSize, rendered_values.get(), &dwBufferUsed, &dwPropertyCount);
    if (ERROR_SUCCESS != (status = GetLastError())) {
      return;
    }
  }

  event_timestamp_ = static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemTimeCreated].FileTimeVal;

  SYSTEMTIME st;
  FILETIME ft;

  ft.dwHighDateTime = (DWORD)((event_timestamp_ >> 32) & 0xFFFFFFFF);
  ft.dwLowDateTime = (DWORD)(event_timestamp_ & 0xFFFFFFFF);

  FileTimeToSystemTime(&ft, &st);
  std::stringstream datestr;

  std::string period = "AM";
  auto hour = st.wHour;
  if (hour >= 12 && hour < 24)
    period = "PM";
  if (hour > 12)
    hour -= 12;
  if (hour == 0)
    hour = 12;
  datestr << st.wMonth << "/" << st.wDay << "/" << st.wYear << " " << std::setfill('0') << std::setw(2) << hour << ":" << std::setfill('0')
          << std::setw(2) << st.wMinute << ":" << std::setfill('0') << std::setw(2) << st.wSecond << " " << period;
  event_timestamp_str_ = datestr.str();
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
  std::unique_ptr<WCHAR, Deleter> buffer{ stack_buffer, Deleter{stack_buffer} };
  DWORD num_chars_used = 0;
  DWORD result = 0;

  std::string event_data;

  if (metadata_ptr_ == NULL || event_ptr_ == NULL) {
    return event_data;
  }


  if (!EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, NULL, flags, num_chars_in_buffer, buffer.get(), &num_chars_used)) {
    result = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == result) {
      num_chars_in_buffer = num_chars_used;

      buffer.reset((LPWSTR) malloc(num_chars_in_buffer * sizeof(WCHAR)));
      if (!buffer) {
        return event_data;
      }

      EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, NULL, flags, num_chars_in_buffer, buffer.get(), &num_chars_used);
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

std::string WindowsEventLogHandler::getEventMessage(EVT_HANDLE eventHandle) const {
  std::string returnValue;
  WCHAR stack_buffer[4096];
  DWORD num_chars_in_buffer = sizeof(stack_buffer) / sizeof(stack_buffer[0]);
  using Deleter = utils::StackAwareDeleter<WCHAR, utils::FreeDeleter>;
  std::unique_ptr<WCHAR, Deleter> buffer{ stack_buffer, Deleter{stack_buffer} };
  DWORD num_chars_used = 0;
  DWORD status = 0;

  EvtFormatMessage(metadata_provider_.get(), eventHandle, 0, 0, NULL, EvtFormatMessageEvent, num_chars_in_buffer, buffer.get(), &num_chars_used);
  if (num_chars_used == 0) {
    return returnValue;
  }

  //  we need to get the size of the buffer
  status = GetLastError();
  if (ERROR_INSUFFICIENT_BUFFER == status) {
    num_chars_in_buffer = num_chars_used;

    /* All C++ examples use malloc and even HeapAlloc in some cases. To avoid any problems ( with EvtFormatMessage calling
      free for example ) we will continue to use malloc and use a custom deleter with unique_ptr.
    '*/
    buffer.reset((LPWSTR)malloc(num_chars_in_buffer * sizeof(WCHAR)));
    if (!buffer) {
      return returnValue;
    }

    EvtFormatMessage(metadata_provider_.get(), eventHandle, 0, 0, NULL, EvtFormatMessageEvent, num_chars_in_buffer, buffer.get(), &num_chars_used);
  }

  if (ERROR_EVT_MESSAGE_NOT_FOUND == status || ERROR_EVT_MESSAGE_ID_NOT_FOUND == status) {
    return returnValue;
  }

  // convert wstring to std::string
  return to_string(buffer.get());
}

void WindowsEventLogHeader::setDelimiter(const std::string &delim) {
  delimiter_ = delim;
}

std::string WindowsEventLogHeader::createDefaultDelimiter(size_t max, size_t length) const {
  if (max > length) {
    return ":" + std::string(max - length, ' ');
  } else {
    return ": ";
  }
}

EVT_HANDLE WindowsEventLogHandler::getMetadata() const {
  return metadata_provider_.get();
}

}  // namespace org::apache::nifi::minifi::wel
