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

#include <Windows.h>
#include <winevt.h>
#include <Objbase.h>

#include <sstream>
#include <regex>
#include <codecvt>
#include <mutex>
#include <unordered_map>
#include <tuple>
#include <map>
#include <memory>
#include <string>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/OsUtils.h"
#include "wel/WindowsEventLog.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "pugixml.hpp"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

struct EventRender {
  std::map<std::string, std::string> matched_fields;
  std::string xml;
  std::string plaintext;
  std::string json;
};

class Bookmark;

//! ConsumeWindowsEventLog Class
class ConsumeWindowsEventLog : public core::Processor {
 public:
  //! Constructor
  /*!
  * Create a new processor
  */
  explicit ConsumeWindowsEventLog(const std::string& name, const utils::Identifier& uuid = {});

  //! Destructor
  virtual ~ConsumeWindowsEventLog();

  //! Processor Name
  EXTENSIONAPI static const std::string ProcessorName;

  //! Supported Properties
  EXTENSIONAPI static core::Property Channel;
  EXTENSIONAPI static core::Property Query;
  EXTENSIONAPI static core::Property RenderFormatXML;
  EXTENSIONAPI static core::Property MaxBufferSize;
  EXTENSIONAPI static core::Property InactiveDurationToReconnect;
  EXTENSIONAPI static core::Property IdentifierMatcher;
  EXTENSIONAPI static core::Property IdentifierFunction;
  EXTENSIONAPI static core::Property ResolveAsAttributes;
  EXTENSIONAPI static core::Property EventHeaderDelimiter;
  EXTENSIONAPI static core::Property EventHeader;
  EXTENSIONAPI static core::Property OutputFormat;
  EXTENSIONAPI static core::Property JSONFormat;
  EXTENSIONAPI static core::Property BatchCommitSize;
  EXTENSIONAPI static core::Property BookmarkRootDirectory;
  EXTENSIONAPI static core::Property ProcessOldEvents;

  //! Supported Relationships
  EXTENSIONAPI static core::Relationship Success;

 public:
  /**
  * Function that's executed when the processor is scheduled.
  * @param context process context.
  * @param sessionFactory process session factory that is used when creating
  * ProcessSession objects.
  */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  //! OnTrigger method, implemented by NiFi ConsumeWindowsEventLog
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  //! Initialize, overwrite by NiFi ConsumeWindowsEventLog
  void initialize(void) override;
  void notifyStop() override;

 protected:
  void refreshTimeZoneData();
  void putEventRenderFlowFileToSession(const EventRender& eventRender, core::ProcessSession& session) const;
  wel::WindowsEventLogHandler getEventLogHandler(const std::string & name);
  bool insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string &value) const;
  void LogWindowsError(std::string error = "Error") const;
  bool createEventRender(EVT_HANDLE eventHandle, EventRender& eventRender);
  void substituteXMLPercentageItems(pugi::xml_document& doc);

  static constexpr const char* XML = "XML";
  static constexpr const char* Both = "Both";
  static constexpr const char* Plaintext = "Plaintext";
  static constexpr const char* JSON = "JSON";
  static constexpr const char* JSONRaw = "Raw";
  static constexpr const char* JSONSimple = "Simple";
  static constexpr const char* JSONFlattened = "Flattened";

 private:
  struct TimeDiff {
    auto operator()() const {
      return int64_t{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time_).count() };
    }
    const decltype(std::chrono::steady_clock::now()) time_ = std::chrono::steady_clock::now();
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  bool commitAndSaveBookmark(const std::wstring &bookmarkXml, const std::shared_ptr<core::ProcessSession> &session);
  std::tuple<size_t, std::wstring> processEventLogs(const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::ProcessSession> &session, const EVT_HANDLE& event_query_results);

  // Logger
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  wel::METADATA_NAMES header_names_;
  std::string header_delimiter_;
  std::string channel_;
  std::wstring wstrChannel_;
  std::wstring wstrQuery_;
  std::string regex_;
  bool resolve_as_attributes_{false};
  bool apply_identifier_function_{false};
  std::string provenanceUri_;
  std::string computerName_;
  uint64_t maxBufferSize_{};
  DWORD lastActivityTimestamp_{};
  std::mutex cache_mutex_;
  std::map<std::string, wel::WindowsEventLogHandler > providers_;
  uint64_t batch_commit_size_{};

  enum class JSONType {None, Raw, Simple, Flattened};

  struct OutputFormat {
    bool xml{false};
    bool plaintext{false};
    struct JSON {
      JSONType type{JSONType::None};

      explicit operator bool() const noexcept {
        return type != JSONType::None;
      }
    } json;
  } output_;

  std::unique_ptr<Bookmark> bookmark_;
  std::mutex on_trigger_mutex_;
  std::unordered_map<std::string, std::string> xmlPercentageItemsResolutions_;
  HMODULE hMsobjsDll_{};

  std::string timezone_name_;
  std::string timezone_offset_;  // Represented as UTC offset in (+|-)HH:MM format, like +02:00
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
