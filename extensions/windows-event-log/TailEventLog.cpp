/**
 * @file TailEventLog.cpp
 * TailEventLog class implementation
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

#include "TailEventLog.h"

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string TailEventLog::ProcessorName("TailEventLog");
core::Relationship TailEventLog::Success("success", "All files, containing log events, are routed to success");
core::Property TailEventLog::LogSourceFileName("Log Source", "Log Source from which to read events", "");
core::Property TailEventLog::MaxEventsPerFlowFile("Max Events Per FlowFile", "Events per flow file", "1");
void TailEventLog::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(LogSourceFileName);
  properties.insert(MaxEventsPerFlowFile);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void TailEventLog::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;

  if (context->getProperty(LogSourceFileName.getName(), value)) {
    log_source_ = value;
  }
  if (context->getProperty(MaxEventsPerFlowFile.getName(), value)) {
    core::Property::StringToInt(value, max_events_);
  }

  log_handle_ = OpenEventLog(NULL, log_source_.c_str());

  logger_->log_trace("TailEventLog configured to tail %s", log_source_);
}

void TailEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (log_handle_ == nullptr) {
    logger_->log_debug("Handle could not be created for %s", log_source_);
  }

  BYTE buffer[MAX_RECORD_BUFFER_SIZE];

  EVENTLOGRECORD *event_record = reinterpret_cast<EVENTLOGRECORD*>(&buffer);

  DWORD bytes_to_read = 0, min_bytes = 0;

  GetOldestEventLogRecord(log_handle_, &current_record_);
  GetNumberOfEventLogRecords(log_handle_, &num_records_);
  current_record_ = num_records_-max_events_;

  logger_->log_trace("%d and %d", current_record_, num_records_);

  if (ReadEventLog(log_handle_, EVENTLOG_FORWARDS_READ | EVENTLOG_SEEK_READ, current_record_, event_record, MAX_RECORD_BUFFER_SIZE, &bytes_to_read, &min_bytes)) {
    if (bytes_to_read == 0) {
      logger_->log_debug("Yielding");
      context->yield();
    }
    while (bytes_to_read > 0) {
      auto flowFile = session->create();
      if (flowFile == nullptr)
        return;

      LPSTR source =
        (LPSTR)((LPBYTE)event_record + sizeof(EVENTLOGRECORD));

      LPSTR computer_name =
        (LPSTR)((LPBYTE)event_record + sizeof(EVENTLOGRECORD) +
          strlen(source) + 1);

      flowFile->addAttribute("source", source);
      flowFile->addAttribute("record_number", std::to_string(event_record->RecordNumber));
      flowFile->addAttribute("computer_name", computer_name);

      flowFile->addAttribute("event_time", getTimeStamp(event_record->TimeGenerated));
      flowFile->addAttribute("event_type", typeToString(event_record->EventType));

      io::BufferStream stream((const uint8_t*)(event_record + event_record->DataOffset), event_record->DataLength);
      // need an import from the data stream.
      session->importFrom(stream, flowFile);
      session->transfer(flowFile, Success);
      bytes_to_read -= event_record->Length;
      event_record = reinterpret_cast<EVENTLOGRECORD *>((LPBYTE)event_record + event_record->Length);
    }

    event_record = reinterpret_cast<EVENTLOGRECORD*>(&buffer);
    logger_->log_trace("All done no more");
  } else {
    LogWindowsError();
    logger_->log_trace("Yielding due to error");
    context->yield();
  }
}

REGISTER_RESOURCE(TailEventLog, "Windows event log reader that functions as a stateful tail of the provided windows event log name");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
