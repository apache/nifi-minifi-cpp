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
#include <stdio.h>
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

#include "wel/MetadataWalker.h"
#include "wel/XMLString.h"
#include "wel/UnicodeConversion.h"
#include "wel/JSONUtils.h"

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Bookmark.h"
#include "utils/Deleters.h"

#include "utils/gsl.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// ConsumeWindowsEventLog
const std::string ConsumeWindowsEventLog::ProcessorName("ConsumeWindowsEventLog");
const int EVT_NEXT_TIMEOUT_MS = 500;

core::Property ConsumeWindowsEventLog::Channel(
  core::PropertyBuilder::createProperty("Channel")->
  isRequired(true)->
  withDefaultValue("System")->
  withDescription("The Windows Event Log Channel to listen to.")->
  supportsExpressionLanguage(true)->
  build());

core::Property ConsumeWindowsEventLog::Query(
  core::PropertyBuilder::createProperty("Query")->
  isRequired(true)->
  withDefaultValue("*")->
  withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")->
  supportsExpressionLanguage(true)->
  build());

core::Property ConsumeWindowsEventLog::MaxBufferSize(
  core::PropertyBuilder::createProperty("Max Buffer Size")->
  isRequired(true)->
  withDefaultValue<core::DataSizeValue>("1 MB")->
  withDescription(
    "The individual Event Log XMLs are rendered to a buffer."
    " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")->
  build());

// !!! This property is obsolete since now subscription is not used, but leave since it might be is used already in config.yml.
core::Property ConsumeWindowsEventLog::InactiveDurationToReconnect(
  core::PropertyBuilder::createProperty("Inactive Duration To Reconnect")->
  isRequired(true)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  withDescription(
    "If no new event logs are processed for the specified time period, "
    " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
    " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
    " Setting no duration, e.g. '0 ms' disables auto-reconnection.")->
  build());

core::Property ConsumeWindowsEventLog::IdentifierMatcher(
  core::PropertyBuilder::createProperty("Identifier Match Regex")->
  isRequired(false)->
  withDefaultValue(".*Sid")->
  withDescription("Regular Expression to match Subject Identifier Fields. These will be placed into the attributes of the FlowFile")->
  build());


core::Property ConsumeWindowsEventLog::IdentifierFunction(
  core::PropertyBuilder::createProperty("Apply Identifier Function")->
  isRequired(false)->
  withDefaultValue<bool>(true)->
  withDescription("If true it will resolve SIDs matched in the 'Identifier Match Regex' to the DOMAIN\\USERNAME associated with that ID")->
  build());

core::Property ConsumeWindowsEventLog::ResolveAsAttributes(
  core::PropertyBuilder::createProperty("Resolve Metadata in Attributes")->
  isRequired(false)->
  withDefaultValue<bool>(true)->
  withDescription("If true, any metadata that is resolved ( such as IDs or keyword metadata ) will be placed into attributes, otherwise it will be replaced in the XML or text output")->
  build());


core::Property ConsumeWindowsEventLog::EventHeaderDelimiter(
  core::PropertyBuilder::createProperty("Event Header Delimiter")->
  isRequired(false)->
  withDescription("If set, the chosen delimiter will be used in the Event output header. Otherwise, a colon followed by spaces will be used.")->
  build());


core::Property ConsumeWindowsEventLog::EventHeader(
  core::PropertyBuilder::createProperty("Event Header")->
  isRequired(false)->
  withDefaultValue("LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,"
      "TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType")->
  withDescription("Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,"
      "EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.")->
  build());

core::Property ConsumeWindowsEventLog::OutputFormat(
  core::PropertyBuilder::createProperty("Output Format")->
  isRequired(true)->
  withDefaultValue(Both)->
  withAllowableValues<std::string>({XML, Plaintext, Both, JSON})->
  withDescription("Set the output format type. In case \'Both\' is selected the processor generates two flow files for every event captured in format XML and Plaintext")->
  build());

core::Property ConsumeWindowsEventLog::JSONFormat(
  core::PropertyBuilder::createProperty("JSON Format")->
  isRequired(true)->
  withDefaultValue(JSONSimple)->
  withAllowableValues<std::string>({JSONSimple, JSONFlattened, JSONRaw})->
  withDescription("Set the json format type. Only applicable if Output Format is set to 'JSON'")->
  build());

core::Property ConsumeWindowsEventLog::BatchCommitSize(
  core::PropertyBuilder::createProperty("Batch Commit Size")->
  isRequired(false)->
  withDefaultValue<uint64_t>(1000U)->
  withDescription("Maximum number of Events to consume and create to Flow Files from before committing.")->
  build());

core::Property ConsumeWindowsEventLog::BookmarkRootDirectory(
  core::PropertyBuilder::createProperty("State Directory")->
  isRequired(false)->
  withDefaultValue("CWELState")->
  withDescription("DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.")->
  build());

core::Property ConsumeWindowsEventLog::ProcessOldEvents(
  core::PropertyBuilder::createProperty("Process Old Events")->
  isRequired(true)->
  withDefaultValue<bool>(false)->
  withDescription("This property defines if old events (which are created before first time server is started) should be processed.")->
  build());

core::Relationship ConsumeWindowsEventLog::Success("success", "Relationship for successfully consumed events.");

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, const utils::Identifier& uuid)
  : core::Processor(name, uuid),
    logger_(logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger()) {
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
  //! Set the supported properties
  setSupportedProperties({
     Channel, Query, MaxBufferSize, InactiveDurationToReconnect, IdentifierMatcher, IdentifierFunction, ResolveAsAttributes,
     EventHeaderDelimiter, EventHeader, OutputFormat, JSONFormat, BatchCommitSize, BookmarkRootDirectory, ProcessOldEvents
  });

  //! Set the supported relationships
  setSupportedRelationships({Success});
}

bool ConsumeWindowsEventLog::insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string & value) const {
  wel::METADATA name = wel::WindowsEventLogMetadata::getMetadataFromString(key);

  if (name != wel::METADATA::UNKNOWN) {
    header.emplace_back(std::make_pair(name, value));
    return true;
  }
  return false;
}

void ConsumeWindowsEventLog::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  context->getProperty(IdentifierMatcher.getName(), regex_);
  context->getProperty(ResolveAsAttributes.getName(), resolve_as_attributes_);
  context->getProperty(IdentifierFunction.getName(), apply_identifier_function_);
  context->getProperty(EventHeaderDelimiter.getName(), header_delimiter_);
  context->getProperty(BatchCommitSize.getName(), batch_commit_size_);

  std::string header;
  context->getProperty(EventHeader.getName(), header);

  auto keyValueSplit = utils::StringUtils::split(header, ",");
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

  std::string mode;
  context->getProperty(OutputFormat.getName(), mode);

  output_ = {};
  if (mode == XML) {
    output_.xml = true;
  } else if (mode == Plaintext) {
    output_.plaintext = true;
  } else if (mode == Both) {
    output_.xml = true;
    output_.plaintext = true;
  } else if (mode == JSON) {
    std::string json_format;
    context->getProperty(JSONFormat.getName(), json_format);
    if (json_format == JSONRaw) {
      output_.json.type = JSONType::Raw;
    } else if (json_format == JSONSimple) {
      output_.json.type = JSONType::Simple;
    } else if (json_format == JSONFlattened) {
      output_.json.type = JSONType::Flattened;
    }
  } else {
    // in the future this might be considered an error, but for now due to backwards
    // compatibility we just fall through and execute the processor outputing nothing
    // throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Unrecognized output format: " + mode);
  }

  if ((output_.xml || output_.json) && !hMsobjsDll_) {
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

  context->getProperty(MaxBufferSize.getName(), maxBufferSize_);
  logger_->log_debug("ConsumeWindowsEventLog: maxBufferSize_ %" PRIu64, maxBufferSize_);

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel_ + "?" + query;
  logger_->log_trace("Successfully configured CWEL");
}

bool ConsumeWindowsEventLog::commitAndSaveBookmark(const std::wstring &bookmark_xml, const std::shared_ptr<core::ProcessSession> &session) {
  {
    const TimeDiff time_diff;
    session->commit();
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

std::tuple<size_t, std::wstring> ConsumeWindowsEventLog::processEventLogs(const std::shared_ptr<core::ProcessContext> &context,
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
    EventRender event_render;
    std::wstring new_bookmark_xml;
    if (createEventRender(next_event, event_render) && bookmark_->getNewBookmarkXml(next_event, new_bookmark_xml)) {
      bookmark_xml = std::move(new_bookmark_xml);
      processed_event_count++;
      putEventRenderFlowFileToSession(event_render, *session);
    }
  }
  logger_->log_trace("Finished enumerating events.");
  return std::make_tuple(processed_event_count, bookmark_xml);
}

void ConsumeWindowsEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (!bookmark_) {
    logger_->log_debug("bookmark_ is null");
    context->yield();
    return;
  }

  std::unique_lock<std::mutex> lock(on_trigger_mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
    return;
  }

  logger_->log_trace("CWEL onTrigger");

  size_t processed_event_count = 0;
  const TimeDiff time_diff;
  const auto timeGuard = gsl::finally([&]() {
    logger_->log_debug("processed %zu Events in %"  PRId64 " ms", processed_event_count, time_diff());
  });

  const auto event_query_results = EvtQuery(0, wstrChannel_.c_str(), wstrQuery_.c_str(), EvtQueryChannelPath);
  if (!event_query_results) {
    LOG_LAST_ERROR(EvtQuery);
    context->yield();
    return;
  }
  const auto guard_event_query_results = gsl::finally([event_query_results]() { EvtClose(event_query_results); });

  logger_->log_trace("Retrieved results in Channel: %ls with Query: %ls", wstrChannel_.c_str(), wstrQuery_.c_str());

  auto bookmark_handle = bookmark_->getBookmarkHandleFromXML();
  if (!bookmark_handle) {
    logger_->log_error("bookmark_handle is null, unrecoverable error!");
    bookmark_.reset();
    context->yield();
    return;
  }

  if (!EvtSeek(event_query_results, 1, bookmark_handle, 0, EvtSeekRelativeToBookmark)) {
    LOG_LAST_ERROR(EvtSeek);
    context->yield();
    return;
  }

  refreshTimeZoneData();

  std::wstring bookmark_xml;
  std::tie(processed_event_count, bookmark_xml) = processEventLogs(context, session, event_query_results);

  if (processed_event_count == 0 || !commitAndSaveBookmark(bookmark_xml, session)) {
    context->yield();
    return;
  }
}

wel::WindowsEventLogHandler ConsumeWindowsEventLog::getEventLogHandler(const std::string & name) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  logger_->log_trace("Getting Event Log Handler corresponding to %s", name.c_str());
  auto provider = providers_.find(name);
  if (provider != std::end(providers_)) {
    logger_->log_trace("Found the handler");
    return provider->second;
  }

  std::wstring temp_wstring = std::wstring(name.begin(), name.end());
  LPCWSTR widechar = temp_wstring.c_str();

  providers_[name] = wel::WindowsEventLogHandler(EvtOpenPublisherMetadata(NULL, widechar, NULL, 0, 0));
  logger_->log_trace("Not found the handler -> created handler for %s", name.c_str());
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
    TreeWalker(HMODULE hMsobjsDll, std::unordered_map<std::string, std::string>& xmlPercentageItemsResolutions, std::shared_ptr<logging::Logger> logger)
      : hMsobjsDll_(hMsobjsDll), xmlPercentageItemsResolutions_(xmlPercentageItemsResolutions), logger_(logger) {
    }

    bool for_each(pugi::xml_node& node) override {
      static const std::string percentages = "%%";

      bool percentagesReplaced = false;

      std::string nodeText = node.text().get();

      for (size_t numberPos = 0; std::string::npos != (numberPos = nodeText.find(percentages, numberPos));) {
        numberPos += percentages.size();

        auto number = 0u;
        try {
          // Assumption - first character is not '0', otherwise not all digits will be replaced by 'value'.
          number = std::stoul(&nodeText[numberPos]);
        } catch (const std::invalid_argument &) {
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
                            1024, 0)) {
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
    std::shared_ptr<logging::Logger> logger_;
  } treeWalker(hMsobjsDll_, xmlPercentageItemsResolutions_, logger_);

  doc.traverse(treeWalker);
}

bool ConsumeWindowsEventLog::createEventRender(EVT_HANDLE hEvent, EventRender& eventRender) {
  logger_->log_trace("Rendering an event");
  WCHAR stackBuffer[4096];
  DWORD size = sizeof(stackBuffer);
  using Deleter = utils::StackAwareDeleter<WCHAR, utils::FreeDeleter>;
  std::unique_ptr<WCHAR, Deleter> buf{stackBuffer, Deleter{ stackBuffer }};

  DWORD used = 0;
  DWORD propertyCount = 0;
  if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
      LOG_LAST_ERROR(EvtRender);
      return false;
    }
    if (used > maxBufferSize_) {
      logger_->log_error("Dropping event because it couldn't be rendered within %" PRIu64 " bytes.", maxBufferSize_);
      return false;
    }
    size = used;
    buf.reset((LPWSTR)malloc(size));
    if (!buf) {
      return false;
    }
    if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
      LOG_LAST_ERROR(EvtRender);
      return false;
    }
  }

  logger_->log_debug("Event rendered with size %" PRIu32 ". Performing doc traversing...", used);

  std::string xml = wel::to_string(buf.get());

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (!result) {
    logger_->log_error("Invalid XML produced");
    return false;
  }

  // this is a well known path.
  std::string providerName = doc.child("Event").child("System").child("Provider").attribute("Name").value();
  wel::WindowsEventLogMetadataImpl metadata{getEventLogHandler(providerName).getMetadata(), hEvent};
  wel::MetadataWalker walker{metadata, channel_, !resolve_as_attributes_, apply_identifier_function_, regex_};

  // resolve the event metadata
  doc.traverse(walker);

  logger_->log_debug("Finish doc traversing, performing writing...");

  if (output_.plaintext) {
    logger_->log_trace("Writing event in plain text");

    auto handler = getEventLogHandler(providerName);
    auto message = handler.getEventMessage(hEvent);

    if (!message.empty()) {
      for (const auto &mapEntry : walker.getIdentifiers()) {
        // replace the identifiers with their translated strings.
        if (mapEntry.first.empty() || mapEntry.second.empty()) {
          continue;  // This is most probably a result of a failed ID resolution
        }
        utils::StringUtils::replaceAll(message, mapEntry.first, mapEntry.second);
      }
      wel::WindowsEventLogHeader log_header(header_names_);
      // set the delimiter
      log_header.setDelimiter(header_delimiter_);
      // render the header.
      eventRender.plaintext = log_header.getEventHeader([&walker](wel::METADATA metadata) { return walker.getMetadata(metadata); });
      eventRender.plaintext += "Message" + header_delimiter_ + " ";
      eventRender.plaintext += message;
    }
    logger_->log_trace("Finish writing in plain text");
  }

  if (output_.xml || output_.json) {
    substituteXMLPercentageItems(doc);
    logger_->log_trace("Finish substituting %% in XML");

    if (resolve_as_attributes_) {
      eventRender.matched_fields = walker.getFieldValues();
    }
  }

  if (output_.xml) {
    logger_->log_trace("Writing event in XML");

    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    xml = writer.xml_;

    eventRender.xml = std::move(xml);
    logger_->log_trace("Finish writing in XML");
  }

  if (output_.json.type == JSONType::Raw) {
    logger_->log_trace("Writing event in raw JSON");
    eventRender.json = wel::jsonToString(wel::toRawJSON(doc));
    logger_->log_trace("Finish writing in raw JSON");
  } else if (output_.json.type == JSONType::Simple) {
    logger_->log_trace("Writing event in simple JSON");
    eventRender.json = wel::jsonToString(wel::toSimpleJSON(doc));
    logger_->log_trace("Finish writing in simple JSON");
  } else if (output_.json.type == JSONType::Flattened) {
    logger_->log_trace("Writing event in flattened JSON");
    eventRender.json = wel::jsonToString(wel::toFlattenedJSON(doc));
    logger_->log_trace("Finish writing in flattened JSON");
  }

  return true;
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
      // [[fallthrough]];
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
  struct WriteCallback : public OutputStreamCallback {
    explicit WriteCallback(const std::string& str)
        : str_(str) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
      const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(str_.c_str()), str_.size());
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }

    const std::string& str_;
  };

  auto commitFlowFile = [&] (const std::shared_ptr<core::FlowFile>& flowFile, const std::string& content, const std::string& mimeType) {
    {
      WriteCallback wc{ content };
      session.write(flowFile, &wc);
    }
    session.putAttribute(flowFile, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
    session.putAttribute(flowFile, "Timezone name", timezone_name_);
    session.putAttribute(flowFile, "Timezone offset", timezone_offset_);
    session.getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session.transfer(flowFile, Success);
  };

  if (output_.xml) {
    auto flowFile = session.create();
    logger_->log_trace("Writing rendered XML to a flow file");

    for (const auto &fieldMapping : eventRender.matched_fields) {
      if (!fieldMapping.second.empty()) {
        session.putAttribute(flowFile, fieldMapping.first, fieldMapping.second);
      }
    }

    commitFlowFile(flowFile, eventRender.xml, "application/xml");
  }

  if (output_.plaintext) {
    logger_->log_trace("Writing rendered plain text to a flow file");
    commitFlowFile(session.create(), eventRender.plaintext, "text/plain");
  }

  if (output_.json.type == JSONType::Raw) {
    logger_->log_trace("Writing rendered raw JSON to a flow file");
    commitFlowFile(session.create(), eventRender.json, "application/json");
  } else if (output_.json.type == JSONType::Simple) {
    logger_->log_trace("Writing rendered simple JSON to a flow file");
    commitFlowFile(session.create(), eventRender.json, "application/json");
  } else if (output_.json.type == JSONType::Flattened) {
    logger_->log_trace("Writing rendered flattened JSON to a flow file");
    commitFlowFile(session.create(), eventRender.json, "application/json");
  }
}

void ConsumeWindowsEventLog::LogWindowsError(std::string error) const {
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

  logger_->log_error((error + " %x: %s\n").c_str(), static_cast<int>(error_id), reinterpret_cast<char *>(lpMsg));

  LocalFree(lpMsg);
}

REGISTER_RESOURCE(ConsumeWindowsEventLog, "Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
