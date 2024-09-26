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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MAX_RECORD_BUFFER_SIZE 0x10000  // 64k
const LPCWSTR pEventTypeNames[] = { L"Error", L"Warning", L"Informational", L"Audit Success", L"Audit Failure" };
const char log_name[255] = "Application";

class TailEventLog : public core::ProcessorImpl {
 public:
  explicit TailEventLog(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  virtual ~TailEventLog() = default;

  EXTENSIONAPI static constexpr const char* Description = "Windows event log reader that functions as a stateful tail of the provided windows event log name";

  EXTENSIONAPI static constexpr auto LogSourceFileName = core::PropertyDefinitionBuilder<>::createProperty("Log Source")
      .withDescription("Log Source from which to read events")
      .build();
  EXTENSIONAPI static constexpr auto MaxEventsPerFlowFile = core::PropertyDefinitionBuilder<>::createProperty("Max Events Per FlowFile")
      .withDescription("Events per flow file")
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      LogSourceFileName,
      MaxEventsPerFlowFile
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files, containing log events, are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
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

 private:
  std::mutex log_mutex_;
  std::string log_source_;
  uint32_t max_events_ = 1;
  DWORD current_record_;
  DWORD num_records_;

  HANDLE log_handle_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TailEventLog>::getLogger();
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
