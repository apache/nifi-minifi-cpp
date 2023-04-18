/**
 * @file ConsumeWindowsEventLog.cpp
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

#include "ConsumeWindowsEventLog.h"
#include <cstdio>
#include <vector>
#include <tuple>
#include <utility>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <iostream>
#include <memory>
#include <regex>
#include <cinttypes>

#include "wel/LookupCacher.h"
#include "wel/MetadataWalker.h"
#include "wel/XMLString.h"
#include "wel/UnicodeConversion.h"
#include "wel/JSONUtils.h"

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "Bookmark.h"
#include "wel/UniqueEvtHandle.h"
#include "utils/Deleters.h"
#include "logging/LoggerConfiguration.h"

#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "utils/ProcessorConfigUtils.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

const int EVT_NEXT_TIMEOUT_MS = 500;

const core::Property ConsumeWindowsEventLog::Channel(
  core::PropertyBuilder::createProperty("Channel")->
  isRequired(true)->
  withDefaultValue("System")->
  withDescription("The Windows Event Log Channel to listen to.")->
  supportsExpressionLanguage(true)->
  build());

const core::Property ConsumeWindowsEventLog::Query(
  core::PropertyBuilder::createProperty("Query")->
  isRequired(true)->
  withDefaultValue("*")->
  withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")->
  supportsExpressionLanguage(true)->
  build());

const core::Property ConsumeWindowsEventLog::MaxBufferSize(
  core::PropertyBuilder::createProperty("Max Buffer Size")->
  isRequired(true)->
  withDefaultValue<core::DataSizeValue>("1 MB")->
  withDescription(
    "The individual Event Log XMLs are rendered to a buffer."
    " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")->
  build());

// !!! This property is obsolete since now subscription is not used, but leave since it might be is used already in config.yml.
const core::Property ConsumeWindowsEventLog::InactiveDurationToReconnect(
  core::PropertyBuilder::createProperty("Inactive Duration To Reconnect")->
  isRequired(true)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  withDescription(
    "If no new event logs are processed for the specified time period, "
    " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
    " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
    " Setting no duration, e.g. '0 ms' disables auto-reconnection.")->
  build());

const core::Property ConsumeWindowsEventLog::IdentifierMatcher(
  core::PropertyBuilder::createProperty("Identifier Match Regex")->
  isRequired(false)->
  withDefaultValue(".*Sid")->
  withDescription("Regular Expression to match Subject Identifier Fields. These will be placed into the attributes of the FlowFile")->
  build());


const core::Property ConsumeWindowsEventLog::IdentifierFunction(
  core::PropertyBuilder::createProperty("Apply Identifier Function")->
  isRequired(false)->
  withDefaultValue<bool>(true)->
  withDescription("If true it will resolve SIDs matched in the 'Identifier Match Regex' to the DOMAIN\\USERNAME associated with that ID")->
  build());

const core::Property ConsumeWindowsEventLog::ResolveAsAttributes(
  core::PropertyBuilder::createProperty("Resolve Metadata in Attributes")->
  isRequired(false)->
  withDefaultValue<bool>(true)->
  withDescription("If true, any metadata that is resolved ( such as IDs or keyword metadata ) will be placed into attributes, otherwise it will be replaced in the XML or text output")->
  build());


const core::Property ConsumeWindowsEventLog::EventHeaderDelimiter(
  core::PropertyBuilder::createProperty("Event Header Delimiter")->
  isRequired(false)->
  withDescription("If set, the chosen delimiter will be used in the Event output header. Otherwise, a colon followed by spaces will be used.")->
  build());


const core::Property ConsumeWindowsEventLog::EventHeader(
  core::PropertyBuilder::createProperty("Event Header")->
  isRequired(false)->
  withDefaultValue("LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,"
      "TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType")->
  withDescription("Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,"
      "EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.")->
  build());

const core::Property ConsumeWindowsEventLog::OutputFormatProperty(
  core::PropertyBuilder::createProperty("Output Format")->
  isRequired(true)->
  withDefaultValue(toString(OutputFormat::BOTH))->
  withAllowableValues(OutputFormat::values())->
  withDescription("Set the output format type. In case \'Both\' is selected the processor generates two flow files for every event captured in format XML and Plaintext")->
  build());

const core::Property ConsumeWindowsEventLog::JsonFormatProperty(
  core::PropertyBuilder::createProperty("JSON Format")->
  isRequired(true)->
  withDefaultValue(toString(JsonFormat::SIMPLE))->
  withAllowableValues(JsonFormat::values())->
  withDescription("Set the json format type. Only applicable if Output Format is set to 'JSON'")->
  build());

const core::Property ConsumeWindowsEventLog::BatchCommitSize(
  core::PropertyBuilder::createProperty("Batch Commit Size")->
  isRequired(false)->
  withDefaultValue<uint64_t>(1000U)->
  withDescription("Maximum number of Events to consume and create to Flow Files from before committing.")->
  build());

const core::Property ConsumeWindowsEventLog::BookmarkRootDirectory(
  core::PropertyBuilder::createProperty("State Directory")->
  isRequired(false)->
  withDefaultValue("CWELState")->
  withDescription("DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.")->
  build());

const core::Property ConsumeWindowsEventLog::ProcessOldEvents(
  core::PropertyBuilder::createProperty("Process Old Events")->
  isRequired(true)->
  withDefaultValue<bool>(false)->
  withDescription("This property defines if old events (which are created before first time server is started) should be processed.")->
  build());

const core::Property ConsumeWindowsEventLog::CacheSidLookups(
    core::PropertyBuilder::createProperty("Cache SID Lookups")->
        isRequired(false)->
        withDefaultValue<bool>(true)->
        withDescription("Determines whether SID to name lookups are cached in memory")->
        build());

const core::Relationship ConsumeWindowsEventLog::Success("success", "Relationship for successfully consumed events.");

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, const utils::Identifier& uuid)
  : core::Processor(name, uuid),
    logger_(core::logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger(uuid_)) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LogWindowsError();
  }
}

void ConsumeWindowsEventLog::notifyStop() {
  std::lock_guard<std::mutex> lock(on_trigger_mutex_);
  logger_->log_trace("start notifyStop");
  bookmark_.reset();
  if (hMsobjsDll_) {
    if (FreeLibrary(hMsobjsDll_)) {
      hMsobjsDll_ = nullptr;
    } else {
      LOG_LAST_ERROR(LoadLibrary);
    }
  }
  logger_->log_trace("finish notifyStop");
}

ConsumeWindowsEventLog::~ConsumeWindowsEventLog() {
  if (hMsobjsDll_) {
    FreeLibrary(hMsobjsDll_);
  }
}

void ConsumeWindowsEventLog::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

bool ConsumeWindowsEventLog::insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string & value) {
  wel::METADATA name = wel::WindowsEventLogMetadata::getMetadataFromString(key);

  if (name != wel::METADATA::UNKNOWN) {
    header.emplace_back(std::make_pair(name, value));
    return true;
  }
  return false;
}

void ConsumeWindowsEventLog::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  context->getProperty(ResolveAsAttributes.getName(), resolve_as_attributes_);
  context->getProperty(IdentifierFunction.getName(), apply_identifier_function_);
  header_delimiter_ = context->getProperty(EventHeaderDelimiter);
  context->getProperty(BatchCommitSize.getName(), batch_commit_size_);

  header_names_.clear();
  if (auto header = context->getProperty(EventHeader)) {
    auto keyValueSplit = utils::StringUtils::split(*header, ",");
    for (const auto &kv : keyValueSplit) {
      auto splitKeyAndValue = utils::StringUtils::split(kv, "=");
      if (splitKeyAndValue.size() == 2) {
        auto key = utils::StringUtils::trim(splitKeyAndValue.at(0));
        auto value = utils::StringUtils::trim(splitKeyAndValue.at(1));
        if (!insertHeaderName(header_names_, key, value)) {
          logger_->log_error("%s is an invalid key for the header map", key);
        }
      } else if (splitKeyAndValue.size() == 1) {
        auto key = utils::StringUtils::trim(splitKeyAndValue.at(0));
        if (!insertHeaderName(header_names_, key, "")) {
          logger_->log_error("%s is an invalid key for the header map", key);
        }
      }
    }
  }

  regex_.reset();
  if (auto identifier_matcher = context->getProperty(IdentifierMatcher); identifier_matcher && !identifier_matcher->empty()) {
    regex_.emplace(*identifier_matcher);
  }

  output_format_ = utils::parseEnumProperty<OutputFormat>(*context, OutputFormatProperty);
  json_format_ = utils::parseEnumProperty<JsonFormat>(*context, JsonFormatProperty);

  if (output_format_ != OutputFormat::PLAINTEXT && !hMsobjsDll_) {
    char systemDir[MAX_PATH];
    if (GetSystemDirectory(systemDir, sizeof(systemDir))) {
      hMsobjsDll_ = LoadLibrary((systemDir + std::string("\\msobjs.dll")).c_str());
      if (!hMsobjsDll_) {
        LOG_LAST_ERROR(LoadLibrary);
      }
    } else {
      LOG_LAST_ERROR(GetSystemDirectory);
    }
  }

  context->getProperty(Channel.getName(), channel_);
  wstrChannel_ = std::wstring(channel_.begin(), channel_.end());

  std::string query;
  context->getProperty(Query.getName(), query);
  wstrQuery_ = std::wstring(query.begin(), query.end());

  bool processOldEvents{};
  context->getProperty(ProcessOldEvents.getName(), processOldEvents);

  if (!bookmark_) {
    std::string bookmarkDir;
    context->getProperty(BookmarkRootDirectory.getName(), bookmarkDir);
    if (bookmarkDir.empty()) {
      logger_->log_error("State Directory is empty");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "State Directory is empty");
    }
    bookmark_ = std::make_unique<Bookmark>(wstrChannel_, wstrQuery_, bookmarkDir, getUUID(), processOldEvents, state_manager_, logger_);
    if (!*bookmark_) {
      bookmark_.reset();
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bookmark is empty");
    }
  }

  context->getProperty(MaxBufferSize.getName(), max_buffer_size_);
  logger_->log_debug("ConsumeWindowsEventLog: MaxBufferSize %" PRIu64, max_buffer_size_);

  context->getProperty(CacheSidLookups.getName(), cache_sid_lookups_);
  logger_->log_debug("ConsumeWindowsEventLog: will%s cache SID to name lookups", cache_sid_lookups_ ? "" : " not");

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel_ + "?" + query;
  logger_->log_trace("Successfully configured CWEL");
}

bool ConsumeWindowsEventLog::commitAndSaveBookmark(const std::wstring &bookmark_xml, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  {
    const TimeDiff time_diff;
    session->commit();
    context->getStateManager()->beginTransaction();
    logger_->log_debug("processQueue commit took %" PRId64 " ms", time_diff());
  }

  if (!bookmark_->saveBookmarkXml(bookmark_xml)) {
    logger_->log_error("Failed to save bookmark xml");
  }

  if (session->outgoingConnectionsFull("success")) {
    logger_->log_debug("Outgoing success connection is full");
    return false;
  }

  return true;
}

std::tuple<size_t, std::wstring> ConsumeWindowsEventLog::processEventLogs(const std::shared_ptr<core::ProcessContext>&,
    const std::shared_ptr<core::ProcessSession> &session, const EVT_HANDLE& event_query_results) {
  size_t processed_event_count = 0;
  std::wstring bookmark_xml;
  logger_->log_trace("Enumerating the events in the result set after the bookmarked event.");
  while (processed_event_count < batch_commit_size_ || batch_commit_size_ == 0) {
    EVT_HANDLE next_event{};
    DWORD handles_set_count{};
    if (!EvtNext(event_query_results, 1, &next_event, EVT_NEXT_TIMEOUT_MS, 0, &handles_set_count)) {
      if (ERROR_NO_MORE_ITEMS != GetLastError()) {
        LogWindowsError("Failed to get next event");
        continue;
        /* According to MS this iteration should only end when the return value is false AND
          the error code is NO_MORE_ITEMS. See the following page for further details:
          https://docs.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtnext */
      }
      break;
    }

    const auto guard_next_event = gsl::finally([next_event]() { EvtClose(next_event); });
    logger_->log_trace("Succesfully got the next event, performing event rendering");
    auto event_render = createEventRender(next_event);
    if (!event_render) {
      logger_->log_error("%s", event_render.error());
      continue;
    }
    auto new_bookmark_xml = bookmark_->getNewBookmarkXml(next_event);
    if (!new_bookmark_xml) {
      logger_->log_error("%s", new_bookmark_xml.error());
      continue;
    }
    bookmark_xml = std::move(*new_bookmark_xml);
    processed_event_count++;
    putEventRenderFlowFileToSession(*event_render, *session);
  }
  logger_->log_trace("Finished enumerating events.");
  return std::make_tuple(processed_event_count, bookmark_xml);
}

void ConsumeWindowsEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(on_trigger_mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
    return;
  }

  if (!bookmark_) {
    logger_->log_debug("bookmark_ is null");
    context->yield();
    return;
  }

  logger_->log_trace("CWEL onTrigger");

  size_t processed_event_count = 0;
  const TimeDiff time_diff;
  const auto timeGuard = gsl::finally([&]() {
    logger_->log_debug("processed %zu Events in %"  PRId64 " ms", processed_event_count, time_diff());
  });

  wel::unique_evt_handle event_query_results{EvtQuery(nullptr, wstrChannel_.c_str(), wstrQuery_.c_str(), EvtQueryChannelPath)};
  if (!event_query_results) {
    LOG_LAST_ERROR(EvtQuery);
    context->yield();
    return;
  }

  logger_->log_trace("Retrieved results in Channel: %ls with Query: %ls", wstrChannel_.c_str(), wstrQuery_.c_str());

  auto bookmark_handle = bookmark_->getBookmarkHandleFromXML();
  if (!bookmark_handle) {
    logger_->log_error("bookmark_handle is null, unrecoverable error!");
    bookmark_.reset();
    context->yield();
    return;
  }

  if (!EvtSeek(event_query_results.get(), 1, bookmark_handle, 0, EvtSeekRelativeToBookmark)) {
    LOG_LAST_ERROR(EvtSeek);
    context->yield();
    return;
  }

  refreshTimeZoneData();

  std::wstring bookmark_xml;
  std::tie(processed_event_count, bookmark_xml) = processEventLogs(context, session, event_query_results.get());

  if (processed_event_count == 0 || !commitAndSaveBookmark(bookmark_xml, context, session)) {
    context->yield();
    return;
  }
}

wel::WindowsEventLogHandler& ConsumeWindowsEventLog::getEventLogHandler(const std::string & name) {
  logger_->log_trace("Getting Event Log Handler corresponding to %s", name.c_str());
  auto provider = providers_.find(name);
  if (provider != std::end(providers_)) {
    logger_->log_trace("Found the handler");
    return provider->second;
  }

  std::wstring temp_wstring = std::wstring(name.begin(), name.end());
  LPCWSTR widechar = temp_wstring.c_str();

  auto opened_publisher_metadata_provider = EvtOpenPublisherMetadata(nullptr, widechar, nullptr, 0, 0);
  if (!opened_publisher_metadata_provider)
    logger_->log_warn("EvtOpenPublisherMetadata failed due to %s", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message());
  providers_[name] = wel::WindowsEventLogHandler(opened_publisher_metadata_provider);
  logger_->log_info("Handler not found for %s, creating. Number of cached handlers: %zu", name, providers_.size());
  return providers_[name];
}

// !!! Used a non-documented approach to resolve `%%` in XML via C:\Windows\System32\MsObjs.dll.
// Links which mention this approach:
// https://social.technet.microsoft.com/Forums/Windows/en-US/340632d1-60f0-4cc5-ad6f-f8c841107d0d/translate-value-1833quot-on-impersonationlevel-and-similar-values?forum=winservergen
// https://github.com/libyal/libevtx/blob/master/documentation/Windows%20XML%20Event%20Log%20(EVTX).asciidoc
// https://stackoverflow.com/questions/33498244/marshaling-a-message-table-resource
//
// Traverse xml and check each node, if it starts with '%%' and contains only digits, use it as key to lookup value in C:\Windows\System32\MsObjs.dll.
void ConsumeWindowsEventLog::substituteXMLPercentageItems(pugi::xml_document& doc) {
  if (!hMsobjsDll_) {
    return;
  }

  struct TreeWalker: public pugi::xml_tree_walker {
    TreeWalker(HMODULE hMsobjsDll, std::unordered_map<std::string, std::string>& xmlPercentageItemsResolutions, std::shared_ptr<core::logging::Logger> logger)
      : hMsobjsDll_(hMsobjsDll), xmlPercentageItemsResolutions_(xmlPercentageItemsResolutions), logger_(std::move(logger)) {
    }

    bool for_each(pugi::xml_node& node) override {
      static const std::string percentages = "%%";

      bool percentagesReplaced = false;

      std::string nodeText = node.text().get();

      for (size_t numberPos = 0; std::string::npos != (numberPos = nodeText.find(percentages, numberPos));) {
        numberPos += percentages.size();

        DWORD number{};
        try {
          // Assumption - first character is not '0', otherwise not all digits will be replaced by 'value'.
          number = std::stoul(&nodeText[numberPos]);
        } catch (const std::invalid_argument&) {
          continue;
        }

        const std::string key = std::to_string(number);

        std::string value;
        const auto it = xmlPercentageItemsResolutions_.find(key);
        if (it == xmlPercentageItemsResolutions_.end()) {
          LPTSTR pBuffer{};
          if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                            hMsobjsDll_,
                            number,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPTSTR)&pBuffer,
                            1024, nullptr)) {
            value = pBuffer;
            LocalFree(pBuffer);

            value = utils::StringUtils::trimRight(value);

            xmlPercentageItemsResolutions_.insert({key, value});
          } else {
            // Add "" to xmlPercentageItemsResolutions_ - don't need to call FormaMessage for this 'key' again.
            xmlPercentageItemsResolutions_.insert({key, ""});

            logger_->log_error("!FormatMessage error: %x. '%s' is not found in msobjs.dll.", GetLastError(), key.c_str());
          }
        } else {
          value = it->second;
        }

        if (!value.empty()) {
          nodeText.replace(numberPos - percentages.size(), key.size() + percentages.size(), value);

          percentagesReplaced = true;
        }
      }

      if (percentagesReplaced) {
        node.text().set(nodeText.c_str());
      }

      return true;
    }

   private:
    HMODULE hMsobjsDll_;
    std::unordered_map<std::string, std::string>& xmlPercentageItemsResolutions_;
    std::shared_ptr<core::logging::Logger> logger_;
  } treeWalker(hMsobjsDll_, xmlPercentageItemsResolutions_, logger_);

  doc.traverse(treeWalker);
}

nonstd::expected<std::string, std::string> ConsumeWindowsEventLog::renderEventAsXml(EVT_HANDLE event_handle) {
  logger_->log_trace("Rendering an event");
  WCHAR stackBuffer[4096];
  DWORD size = sizeof(stackBuffer);
  using Deleter = utils::StackAwareDeleter<WCHAR, utils::FreeDeleter>;
  std::unique_ptr<WCHAR, Deleter> buf{stackBuffer, Deleter{stackBuffer}};

  DWORD used = 0;
  DWORD propertyCount = 0;
  if (!EvtRender(nullptr, event_handle, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
    DWORD last_error = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER != last_error) {
      std::string error_message = fmt::format("EvtRender failed due to %s", utils::OsUtils::windowsErrorToErrorCode(last_error).message());
      return nonstd::make_unexpected(error_message);
    }
    if (used > max_buffer_size_) {
      std::string error_message = fmt::format("Dropping event because it couldn't be rendered within %" PRIu64 " bytes.", max_buffer_size_);
      return nonstd::make_unexpected(error_message);
    }
    size = used;
    buf.reset((LPWSTR) malloc(size));
    if (!buf)
      return nonstd::make_unexpected("malloc failed");
    if (!EvtRender(nullptr, event_handle, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
      std::string error_message = fmt::format("EvtRender failed due to %s", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message());
      return nonstd::make_unexpected(error_message);
    }
  }
  logger_->log_trace("Event rendered with size %" PRIu32, used);
  return wel::to_string(buf.get());
}

nonstd::expected<EventRender, std::string> ConsumeWindowsEventLog::createEventRender(EVT_HANDLE hEvent) {
  auto event_as_xml = renderEventAsXml(hEvent);
  if (!event_as_xml)
    return nonstd::make_unexpected(event_as_xml.error());

  pugi::xml_document doc;
  if (!doc.load_string(event_as_xml->c_str()))
    return nonstd::make_unexpected("Invalid XML produced");

  EventRender result;

  // this is a well known path.
  std::string provider_name = doc.child("Event").child("System").child("Provider").attribute("Name").value();
  wel::WindowsEventLogMetadataImpl metadata{getEventLogHandler(provider_name).getMetadata(), hEvent};
  wel::MetadataWalker walker{metadata, channel_, !resolve_as_attributes_, apply_identifier_function_, regex_ ? &*regex_ : nullptr, userIdToUsernameFunction()};

  // resolve the event metadata
  doc.traverse(walker);

  logger_->log_trace("Finish doc traversing, performing writing...");

  if (output_format_ == OutputFormat::PLAINTEXT || output_format_ == OutputFormat::BOTH) {
    logger_->log_trace("Writing event in plain text");

    auto& handler = getEventLogHandler(provider_name);
    auto event_message = handler.getEventMessage(hEvent);

    if (event_message) {
      for (const auto& map_entry : walker.getIdentifiers()) {
        if (map_entry.first.empty() || map_entry.second.empty()) {
          continue;
        }
        utils::StringUtils::replaceAll(*event_message, map_entry.first, map_entry.second);
      }
    }

    std::string_view payload_name = event_message ? "Message" : "Error";

    wel::WindowsEventLogHeader log_header(header_names_, header_delimiter_, payload_name.size());
    result.plaintext = log_header.getEventHeader([&walker](wel::METADATA metadata) { return walker.getMetadata(metadata); });
    result.plaintext += payload_name;
    result.plaintext += log_header.getDelimiterFor(payload_name.size());
    result.plaintext += event_message.has_value() ? *event_message : event_message.error().message();

    logger_->log_trace("Finish writing in plain text");
  }

  if (output_format_ != OutputFormat::PLAINTEXT) {
    substituteXMLPercentageItems(doc);
    logger_->log_trace("Finish substituting %% in XML");
  }

  if (resolve_as_attributes_) {
    result.matched_fields = walker.getFieldValues();
  }

  if (output_format_ == OutputFormat::XML || output_format_ == OutputFormat::BOTH) {
    logger_->log_trace("Writing event in XML");

    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    event_as_xml = writer.xml_;

    result.xml = std::move(*event_as_xml);
    logger_->log_trace("Finish writing in XML");
  }

  if (output_format_ == OutputFormat::JSON) {
    switch (json_format_.value()) {
      case JsonFormat::RAW: {
        logger_->log_trace("Writing event in raw JSON");
        result.json = wel::jsonToString(wel::toRawJSON(doc));
        logger_->log_trace("Finish writing in raw JSON");
        break;
      }
      case JsonFormat::SIMPLE: {
        logger_->log_trace("Writing event in simple JSON");
        result.json = wel::jsonToString(wel::toSimpleJSON(doc));
        logger_->log_trace("Finish writing in simple JSON");
        break;
      }
      case JsonFormat::FLATTENED: {
        logger_->log_trace("Writing event in flattened JSON");
        result.json = wel::jsonToString(wel::toFlattenedJSON(doc));
        logger_->log_trace("Finish writing in flattened JSON");
        break;
      }
      default: {
        gsl_Assert(false);
      }
    }
  }

  return result;
}

void ConsumeWindowsEventLog::refreshTimeZoneData() {
  DYNAMIC_TIME_ZONE_INFORMATION tzinfo;
  auto ret = GetDynamicTimeZoneInformation(&tzinfo);
  std::wstring tzstr;
  long tzbias = 0;  // NOLINT long comes from WINDOWS API
  bool dst = false;
  switch (ret) {
    case TIME_ZONE_ID_INVALID:
      logger_->log_error("Failed to get timezone information!");
      return;  // Don't update members in case we cannot get data
    case TIME_ZONE_ID_UNKNOWN:
      tzstr = tzinfo.StandardName;
      tzbias = tzinfo.Bias;
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      tzstr = tzinfo.DaylightName;
      dst = true;
      [[fallthrough]];
    case TIME_ZONE_ID_STANDARD:
      tzstr = tzstr.empty() ? tzinfo.StandardName : tzstr;  // Use standard timezome name in case there is no daylight name or in case it's not DST
      tzbias = tzinfo.Bias + (dst ? tzinfo.DaylightBias : tzinfo.StandardBias);
      break;
  }

  tzbias *= -1;  // WinApi specifies UTC = localtime + bias, but we need offset from UTC
  std::stringstream tzoffset;
  tzoffset << (tzbias >= 0 ? '+' : '-') << std::setfill('0') << std::setw(2) << std::abs(tzbias) / 60
      << ":" << std::setfill('0') << std::setw(2) << std::abs(tzbias) % 60;

  timezone_name_ = wel::to_string(tzstr.c_str());
  timezone_offset_ = tzoffset.str();

  logger_->log_trace("Timezone name: %s, offset: %s", timezone_name_, timezone_offset_);
}

void ConsumeWindowsEventLog::putEventRenderFlowFileToSession(const EventRender& eventRender, core::ProcessSession& session) const {
  auto commitFlowFile = [&] (const std::string& content, const std::string& mimeType) {
    auto flow_file = session.create();
    addMatchedFieldsAsAttributes(eventRender, session, flow_file);
    session.writeBuffer(flow_file, content);
    session.putAttribute(flow_file, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
    session.putAttribute(flow_file, "timezone.name", timezone_name_);
    session.putAttribute(flow_file, "timezone.offset", timezone_offset_);
    session.getProvenanceReporter()->receive(flow_file, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0ms);
    session.transfer(flow_file, Success);
  };

  if (output_format_ == OutputFormat::XML || output_format_ == OutputFormat::BOTH) {
    logger_->log_trace("Writing rendered XML to a flow file");
    commitFlowFile(eventRender.xml, "application/xml");
  }

  if (output_format_ == OutputFormat::PLAINTEXT || output_format_ == OutputFormat::BOTH) {
    logger_->log_trace("Writing rendered plain text to a flow file");
    commitFlowFile(eventRender.plaintext, "text/plain");
  }

  if (output_format_ == OutputFormat::JSON) {
    logger_->log_trace("Writing rendered %s JSON to a flow file", json_format_.toString());
    commitFlowFile(eventRender.json, "application/json");
  }
}

void ConsumeWindowsEventLog::addMatchedFieldsAsAttributes(const EventRender& eventRender, core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flowFile) const {
  for (const auto &fieldMapping : eventRender.matched_fields) {
    if (!fieldMapping.second.empty()) {
      session.putAttribute(flowFile, fieldMapping.first, fieldMapping.second);
    }
  }
}

void ConsumeWindowsEventLog::LogWindowsError(const std::string& error) const {
  auto error_id = GetLastError();
  LPVOID lpMsg;

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM,
    nullptr,
    error_id,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&lpMsg,
    0, nullptr);

  logger_->log_error((error + " %x: %s\n").c_str(), static_cast<int>(error_id), reinterpret_cast<char *>(lpMsg));

  LocalFree(lpMsg);
}

std::function<std::string(const std::string&)> ConsumeWindowsEventLog::userIdToUsernameFunction() const {
  static constexpr auto lookup = &utils::OsUtils::userIdToUsername;
  if (cache_sid_lookups_) {
    static auto cached_lookup = wel::LookupCacher{lookup};
    return std::ref(cached_lookup);
  } else {
    return lookup;
  }
}

REGISTER_RESOURCE(ConsumeWindowsEventLog, Processor);

}  // namespace org::apache::nifi::minifi::processors
