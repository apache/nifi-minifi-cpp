/**
 * @file ConsumeWindowsEventLog.h
 * ConsumeWindowsEventLog class declaration
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
#include "pugixml.hpp"
#include <winevt.h>
#include <sstream>
#include <regex>
#include <codecvt>
#include "utils/OsUtils.h"


//#import <msxml6.dll>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

struct EventRender {
	std::map<std::string, std::string> matched_fields_;
	std::string text_;
};



//! ConsumeWindowsEventLog Class
class ConsumeWindowsEventLog : public core::Processor
{
public:
  //! Constructor
  /*!
  * Create a new processor
  */
  ConsumeWindowsEventLog(const std::string& name, utils::Identifier uuid = utils::Identifier());

  //! Destructor
  virtual ~ConsumeWindowsEventLog();

  //! Processor Name
  static const std::string ProcessorName;

  //! Supported Properties
  static core::Property Channel;
  static core::Property Query;
  static core::Property RenderFormatXML;
  static core::Property MaxBufferSize;
  static core::Property InactiveDurationToReconnect;
  static core::Property IdentifierMatcher;
  static core::Property IdentifierFunction;
  static core::Property ResolveAsAttributes;

  //! Supported Relationships
  static core::Relationship Success;

public:
  /**
  * Function that's executed when the processor is scheduled.
  * @param context process context.
  * @param sessionFactory process session factory that is used when creating
  * ProcessSession objects.
  */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  //! OnTrigger method, implemented by NiFi ConsumeWindowsEventLog
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  //! Initialize, overwrite by NiFi ConsumeWindowsEventLog
  virtual void initialize(void) override;
  virtual void notifyStop() override;

protected:
  bool subscribe(const std::shared_ptr<core::ProcessContext> &context);
  void unsubscribe();
  int processQueue(const std::shared_ptr<core::ProcessSession> &session);
  
  EVT_HANDLE getProvider(const std::string & name);

  void LogWindowsError();
private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  std::string regex_;
  bool resolve_as_attributes_;
  bool apply_identifier_function_;
  moodycamel::ConcurrentQueue<EventRender> listRenderedData_;
  std::string provenanceUri_;
  std::string computerName_;
  int64_t inactiveDurationToReconnect_{};
  EVT_HANDLE subscriptionHandle_{};
  uint64_t maxBufferSize_{};
  DWORD lastActivityTimestamp_{};
  std::shared_ptr<core::ProcessSessionFactory> sessionFactory_;
  std::mutex cache_mutex_;
  std::map<std::string, EVT_HANDLE > providers_;
};

REGISTER_RESOURCE(ConsumeWindowsEventLog, "Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
