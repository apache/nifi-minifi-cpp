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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/OsUtils.h"
#include "wel/WindowsEventLog.h"
#include "wel/EventPath.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "pugixml.hpp"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace cwel {
struct EventRender {
  std::map<std::string, std::string> matched_fields;
  std::string xml;
  std::string plaintext;
  std::string json;
};

enum class OutputFormat {
  XML,
  Both,  // Both is DEPRECATED and removed from the documentation, but kept for backwards compatibility; it means XML + Plaintext
  Plaintext,
  JSON
};

enum class JsonFormat {
  Raw,
  Simple,
  Flattened,
};
}  // namespace cwel

class Bookmark;

class ConsumeWindowsEventLog : public core::ProcessorImpl {
 public:
  explicit ConsumeWindowsEventLog(const std::string& name, const utils::Identifier& uuid = {});

  ~ConsumeWindowsEventLog() override;

  EXTENSIONAPI static constexpr const char* Description = "Registers a Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows. These can be filtered via channel and XPath.";

  EXTENSIONAPI static constexpr auto Channel = core::PropertyDefinitionBuilder<>::createProperty("Channel")
      .isRequired(true)
      .withDefaultValue("System")
      .withDescription("The Windows Event Log Channel to listen to. In order to process logs from a log file use the format 'SavedLog:\\<file path\\>'.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Query = core::PropertyDefinitionBuilder<>::createProperty("Query")
      .isRequired(true)
      .withDefaultValue("*")
      .withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBufferSize = core::PropertyDefinitionBuilder<>::createProperty("Max Buffer Size")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1 MB")
      .withDescription("The individual Event Log XMLs are rendered to a buffer."
          " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")
      .build();
  // !!! This property is obsolete since now subscription is not used, but leave since it might be is used already in config.yml.
  EXTENSIONAPI static constexpr auto InactiveDurationToReconnect = core::PropertyDefinitionBuilder<>::createProperty("Inactive Duration To Reconnect")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .withDescription("If no new event logs are processed for the specified time period, "
          " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
          " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
          " Setting no duration, e.g. '0 ms' disables auto-reconnection.")
      .build();
  EXTENSIONAPI static constexpr auto IdentifierMatcher = core::PropertyDefinitionBuilder<>::createProperty("Identifier Match Regex")
      .isRequired(false)
      .withDefaultValue(".*Sid")
      .withDescription("Regular Expression to match Subject Identifier Fields. These will be placed into the attributes of the FlowFile")
      .build();
  EXTENSIONAPI static constexpr auto IdentifierFunction = core::PropertyDefinitionBuilder<>::createProperty("Apply Identifier Function")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .withDescription("If true it will resolve SIDs matched in the 'Identifier Match Regex' to the DOMAIN\\USERNAME associated with that ID")
      .build();
  EXTENSIONAPI static constexpr auto ResolveAsAttributes = core::PropertyDefinitionBuilder<>::createProperty("Resolve Metadata in Attributes")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .withDescription("If true, any metadata that is resolved ( such as IDs or keyword metadata ) will be placed into attributes, otherwise it will be replaced in the XML or text output")
      .build();
  EXTENSIONAPI static constexpr auto EventHeaderDelimiter = core::PropertyDefinitionBuilder<>::createProperty("Event Header Delimiter")
      .isRequired(false)
      .withDescription("If set, the chosen delimiter will be used in the Event output header. Otherwise, a colon followed by spaces will be used.")
      .build();
  EXTENSIONAPI static constexpr auto EventHeader = core::PropertyDefinitionBuilder<>::createProperty("Event Header")
      .isRequired(false)
      .withDefaultValue("LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,"
          "TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType")
      .withDescription("Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,"
          "EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.")
      .build();
  EXTENSIONAPI static constexpr auto OutputFormatProperty = core::PropertyDefinitionBuilder<magic_enum::enum_count<cwel::OutputFormat>() - 1>::createProperty("Output Format")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(cwel::OutputFormat::XML))
      .withAllowedValues(std::array{magic_enum::enum_name(cwel::OutputFormat::XML), magic_enum::enum_name(cwel::OutputFormat::Plaintext), magic_enum::enum_name(cwel::OutputFormat::JSON)})
      .withDescription("The format of the output flow files.")
      .build();
  EXTENSIONAPI static constexpr auto JsonFormatProperty = core::PropertyDefinitionBuilder<magic_enum::enum_count<cwel::JsonFormat>()>::createProperty("JSON Format")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(cwel::JsonFormat::Simple))
      .withAllowedValues(magic_enum::enum_names<cwel::JsonFormat>())
      .withDescription("Set the json format type. Only applicable if Output Format is set to 'JSON'")
      .build();
  EXTENSIONAPI static constexpr auto BatchCommitSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Commit Size")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("1000")
      .withDescription("Maximum number of Events to consume and create to Flow Files from before committing.")
      .build();
  EXTENSIONAPI static constexpr auto BookmarkRootDirectory = core::PropertyDefinitionBuilder<>::createProperty("State Directory")
      .isRequired(false)
      .withDefaultValue("CWELState")
      .withDescription("DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.")
      .build();
  EXTENSIONAPI static constexpr auto ProcessOldEvents = core::PropertyDefinitionBuilder<>::createProperty("Process Old Events")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .withDescription("This property defines if old events (which are created before first time server is started) should be processed.")
      .build();
  EXTENSIONAPI static constexpr auto CacheSidLookups = core::PropertyDefinitionBuilder<>::createProperty("Cache SID Lookups")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .withDescription("Determines whether SID to name lookups are cached in memory")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
        Channel,
        Query,
        MaxBufferSize,
        InactiveDurationToReconnect,
        IdentifierMatcher,
        IdentifierFunction,
        ResolveAsAttributes,
        EventHeaderDelimiter,
        EventHeader,
        OutputFormatProperty,
        JsonFormatProperty,
        BatchCommitSize,
        BookmarkRootDirectory,
        ProcessOldEvents,
        CacheSidLookups
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Relationship for successfully consumed events."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void notifyStop() override;

 private:
  void refreshTimeZoneData();
  void putEventRenderFlowFileToSession(const cwel::EventRender& eventRender, core::ProcessSession& session) const;
  wel::WindowsEventLogHandler& getEventLogHandler(const std::string& name);
  static bool insertHeaderName(wel::METADATA_NAMES& header, const std::string& key, const std::string& value);
  nonstd::expected<cwel::EventRender, std::string> createEventRender(EVT_HANDLE eventHandle);
  void substituteXMLPercentageItems(pugi::xml_document& doc);
  std::function<std::string(const std::string&)> userIdToUsernameFunction() const;

  nonstd::expected<std::string, std::string> renderEventAsXml(EVT_HANDLE event_handle);

  struct TimeDiff {
    auto operator()() const {
      return std::chrono::steady_clock::now() - time_;
    }
    const decltype(std::chrono::steady_clock::now()) time_ = std::chrono::steady_clock::now();
  };

  bool commitAndSaveBookmark(const std::wstring& bookmarkXml, core::ProcessContext& context, core::ProcessSession& session);

  std::tuple<size_t, std::wstring> processEventLogs(core::ProcessSession& session,
                                                    const EVT_HANDLE& event_query_results);

  void addMatchedFieldsAsAttributes(const cwel::EventRender &eventRender, core::ProcessSession &session, const std::shared_ptr<core::FlowFile> &flowFile) const;

  std::shared_ptr<core::logging::Logger> logger_;
  core::StateManager* state_manager_{nullptr};
  wel::METADATA_NAMES header_names_;
  std::optional<std::string> header_delimiter_;
  wel::EventPath path_;
  std::wstring wstr_query_;
  std::optional<utils::Regex> regex_;
  bool resolve_as_attributes_{false};
  bool apply_identifier_function_{false};
  std::string provenanceUri_;
  std::string computerName_;
  uint64_t max_buffer_size_{};
  std::map<std::string, wel::WindowsEventLogHandler> providers_;
  uint64_t batch_commit_size_{};
  bool cache_sid_lookups_ = true;

  cwel::OutputFormat output_format_;
  cwel::JsonFormat json_format_;

  std::unique_ptr<Bookmark> bookmark_;
  std::mutex on_trigger_mutex_;
  std::unordered_map<std::string, std::string> xmlPercentageItemsResolutions_;
  HMODULE hMsobjsDll_{};

  std::string timezone_name_;
  std::string timezone_offset_;  // Represented as UTC offset in (+|-)HH:MM format, like +02:00
};

}  // namespace org::apache::nifi::minifi::processors
