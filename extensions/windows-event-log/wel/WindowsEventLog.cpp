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
#include "WindowsEventLog.h"
#include "UnicodeConversion.h"
#include "utils/Deleters.h"
#include "utils/ScopeGuard.h"
#include <algorithm>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {


void WindowsEventLogMetadata::renderMetadata() {
  DWORD status = ERROR_SUCCESS;
  DWORD dwBufferSize = 0;
  DWORD dwBufferUsed = 0;
  DWORD dwPropertyCount = 0;
  std::unique_ptr< EVT_VARIANT, utils::FreeDeleter> rendered_values;

  auto context = EvtCreateRenderContext(0, NULL, EvtRenderContextSystem);
  if (context == NULL) {
    return;
  }
  utils::ScopeGuard contextGuard([&context](){
    EvtClose(context);
  });
  if (!EvtRender(context, event_ptr_, EvtRenderEventValues, dwBufferSize, nullptr, &dwBufferUsed, &dwPropertyCount))
  {
    if (ERROR_INSUFFICIENT_BUFFER == (status = GetLastError()))
    {
      dwBufferSize = dwBufferUsed;
      rendered_values = std::unique_ptr<EVT_VARIANT, utils::FreeDeleter>((PEVT_VARIANT)(malloc(dwBufferSize)));
      if (rendered_values)
      {
        EvtRender(context, event_ptr_, EvtRenderEventValues, dwBufferSize, rendered_values.get(), &dwBufferUsed, &dwPropertyCount);
      }
    }
    else {
      return;
    }

    if (ERROR_SUCCESS != (status = GetLastError()))
    {
      return;
    }
  }

  event_timestamp_ = static_cast<PEVT_VARIANT>( rendered_values.get())[EvtSystemTimeCreated].FileTimeVal;

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
  if (hour >= 12)
    hour -= 12;
  datestr << st.wMonth << "/" << st.wDay << "/" << st.wYear << " " << std::setfill('0') << std::setw(2) << hour << ":" << std::setfill('0') << std::setw(2) << st.wMinute << ":" << std::setfill('0') << std::setw(2) << st.wSecond << " " << period;
  event_timestamp_str_ = datestr.str();
  auto level = static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemLevel];
  auto keyword = static_cast<PEVT_VARIANT>(rendered_values.get())[EvtSystemKeywords];
  if (level.Type == EvtVarTypeByte) {
    switch (level.ByteVal)
    {
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
    };

  }
  else {
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

std::string WindowsEventLogHandler::getEventMessage(EVT_HANDLE eventHandle) const
{
  std::string returnValue;
  std::unique_ptr<WCHAR, utils::FreeDeleter> pBuffer;
  DWORD dwBufferSize = 0;
  DWORD dwBufferUsed = 0;
  DWORD status = 0;

  EvtFormatMessage(metadata_provider_, eventHandle, 0, 0, NULL, EvtFormatMessageEvent, dwBufferSize, pBuffer.get(), &dwBufferUsed);
  if (dwBufferUsed == 0) {
    return returnValue;
  }

  //  we need to get the size of the buffer
  status = GetLastError();
  if (ERROR_INSUFFICIENT_BUFFER == status) {
    dwBufferSize = dwBufferUsed;

    /* All C++ examples use malloc and even HeapAlloc in some cases. To avoid any problems ( with EvtFormatMessage calling
      free for example ) we will continue to use malloc and use a custom deleter with unique_ptr.
    '*/
    pBuffer = std::unique_ptr<WCHAR, utils::FreeDeleter>((LPWSTR)malloc(dwBufferSize * sizeof(WCHAR)));
    if (!pBuffer) {
      return returnValue;
    }

    EvtFormatMessage(metadata_provider_, eventHandle, 0, 0, NULL, EvtFormatMessageEvent, dwBufferSize, pBuffer.get(), &dwBufferUsed);
  }

  if (ERROR_EVT_MESSAGE_NOT_FOUND == status || ERROR_EVT_MESSAGE_ID_NOT_FOUND == status) {
    return returnValue;
  }

  // convert wstring to std::string
  return to_string(pBuffer.get());
}

void WindowsEventLogHeader::setDelimiter(const std::string &delim) {
  delimiter_ = delim;
}

std::string WindowsEventLogHeader::createDefaultDelimiter(size_t max, size_t length) const {
  if (max > length) {
    return ":" + std::string(max - length, ' ');
  }
  else {
    return ": ";
  }
}

std::string WindowsEventLogHeader::getEventHeader(const WindowsEventLogMetadata * const metadata) const{
  std::stringstream eventHeader;
  size_t max = 1;
  for (const auto &option : header_names_) {
    max = (std::max(max, option.second.size()));
  }
  ++max; // increment by one to get space.
  for (const auto &option : header_names_) {
    auto name = option.second;
    if (!name.empty()) {
      eventHeader << name << (delimiter_.empty() ? createDefaultDelimiter(max, name.size()) : delimiter_);
    }
    eventHeader << utils::StringUtils::trim(metadata->getMetadata(option.first)) << std::endl;
  }

  return eventHeader.str();
}

EVT_HANDLE WindowsEventLogHandler::getMetadata() const {
	return metadata_provider_;
}

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

