/**
 * @file TailEventLog.h
 * TailEventLog class declaration
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

#include <memory>
#include <string>

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MAX_RECORD_BUFFER_SIZE 0x10000  // 64k
const LPCWSTR pEventTypeNames[] = { L"Error", L"Warning", L"Informational", L"Audit Success", L"Audit Failure" };
const char log_name[255] = "Application";

class TailEventLog : public core::Processor {
 public:
  explicit TailEventLog(const std::string& name, const utils::Identifier& uuid = {})
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<TailEventLog>::getLogger()), max_events_(1) {
  }
  virtual ~TailEventLog() = default;
  static const std::string ProcessorName;

  // Supported Properties
  static core::Property LogSourceFileName;
  static core::Property MaxEventsPerFlowFile;

  // Supported Relationships
  static core::Relationship Success;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  // OnTrigger method, implemented by NiFi TailEventLog
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi TailEventLog
  void initialize(void) override;

 protected:
  void notifyStop() override {
    CloseEventLog(log_handle_);
  }


  inline std::string typeToString(WORD wEventType) {
    switch (wEventType) {
    case EVENTLOG_ERROR_TYPE:
      return "Error";
    case EVENTLOG_WARNING_TYPE:
      return "Warning";
    case EVENTLOG_INFORMATION_TYPE:
      return "Information";
    case EVENTLOG_AUDIT_SUCCESS:
      return "Audit Success";
    case EVENTLOG_AUDIT_FAILURE:
      return "Audit Failure";
    default:
      return "Unknown Event";
    }
  }

  std::string getTimeStamp(const DWORD Time) {
    uint64_t ullTimeStamp = 0;
    uint64_t SecsTo1970 = 116444736000000000;
    SYSTEMTIME st;
    FILETIME ft, ftLocal;

    ullTimeStamp = Int32x32To64(Time, 10000000) + SecsTo1970;
    ft.dwHighDateTime = (DWORD)((ullTimeStamp >> 32) & 0xFFFFFFFF);
    ft.dwLowDateTime = (DWORD)(ullTimeStamp & 0xFFFFFFFF);

    FileTimeToLocalFileTime(&ft, &ftLocal);
    FileTimeToSystemTime(&ftLocal, &st);

    std::stringstream  str;
    str.precision(2);
    str << st.wMonth << "/" << st.wDay << "/" << st.wYear << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond;

    return str.str();
  }

  void LogWindowsError(void) {
    auto error_id = GetLastError();
    LPVOID lpMsg;

    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,
      error_id,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpMsg,
      0, NULL);

    logger_->log_debug("Error %d: %s\n", static_cast<int>(error_id), reinterpret_cast<char *>(lpMsg));

    LocalFree(lpMsg);
  }

 private:
  std::mutex log_mutex_;
  std::string log_source_;
  uint32_t max_events_;
  DWORD current_record_;
  DWORD num_records_;

  HANDLE log_handle_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
