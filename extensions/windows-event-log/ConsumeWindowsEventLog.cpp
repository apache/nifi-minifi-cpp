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
#include "wel/JSONUtils.h"
#include "wel/UniqueEvtHandle.h"

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Bookmark.h"
#include "utils/Deleters.h"
#include "core/logging/LoggerFactory.h"

#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "utils/UnicodeConversion.h"
#include "utils/ProcessorConfigUtils.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

const int EVT_NEXT_TIMEOUT_MS = 500;

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, const utils::Identifier& uuid)
  : core::ProcessorImpl(name, uuid),
    logger_(core::logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger(uuid_)) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    auto last_error = utils::OsUtils::windowsErrorToErrorCode(GetLastError());
    logger_->log_error("{}: {}", last_error, last_error.message());
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
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

bool ConsumeWindowsEventLog::insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string & value) {
  wel::METADATA name = wel::WindowsEventLogMetadata::getMetadataFromString(key);

  if (name != wel::METADATA::UNKNOWN) {
    header.emplace_back(std::make_pair(name, value));
    return true;
  }
  return false;
}

void ConsumeWindowsEventLog::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  context.getProperty(ResolveAsAttributes, resolve_as_attributes_);
  context.getProperty(IdentifierFunction, apply_identifier_function_);
  header_delimiter_ = context.getProperty(EventHeaderDelimiter);
  context.getProperty(BatchCommitSize, batch_commit_size_);

  header_names_.clear();
  if (auto header = context.getProperty(EventHeader)) {
    auto keyValueSplit = utils::string::split(*header, ",");
    for (const auto &kv : keyValueSplit) {
      auto splitKeyAndValue = utils::string::split(kv, "=");
      if (splitKeyAndValue.size() == 2) {
        auto key = utils::string::trim(splitKeyAndValue.at(0));
        auto value = utils::string::trim(splitKeyAndValue.at(1));
        if (!insertHeaderName(header_names_, key, value)) {
          logger_->log_error("{} is an invalid key for the header map", key);
        }
      } else if (splitKeyAndValue.size() == 1) {
        auto key = utils::string::trim(splitKeyAndValue.at(0));
        if (!insertHeaderName(header_names_, key, "")) {
          logger_->log_error("{} is an invalid key for the header map", key);
        }
      }
    }
  }

  regex_.reset();
  if (auto identifier_matcher = context.getProperty(IdentifierMatcher); identifier_matcher && !identifier_matcher->empty()) {
    regex_.emplace(*identifier_matcher);
  }

  output_format_ = utils::parseEnumProperty<cwel::OutputFormat>(context, OutputFormatProperty);
  json_format_ = utils::parseEnumProperty<cwel::JsonFormat>(context, JsonFormatProperty);

  if (output_format_ != cwel::OutputFormat::Plaintext && !hMsobjsDll_) {
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

  path_ = wel::EventPath{context.getProperty(Channel).value()};
  if (path_.kind() == wel::EventPath::Kind::FILE) {
    logger_->log_debug("Using saved log file as log source");
  } else {
    logger_->log_debug("Using channel as log source");
  }

  std::string query;
  context.getProperty(Query, query);
  wstr_query_ = std::wstring(query.begin(), query.end());

  bool processOldEvents{};
  context.getProperty(ProcessOldEvents, processOldEvents);

  if (!bookmark_) {
    std::string bookmarkDir;
    context.getProperty(BookmarkRootDirectory, bookmarkDir);
    if (bookmarkDir.empty()) {
      logger_->log_error("State Directory is empty");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "State Directory is empty");
    }
    bookmark_ = std::make_unique<Bookmark>(path_, wstr_query_, bookmarkDir, getUUID(), processOldEvents, state_manager_, logger_);
    if (!*bookmark_) {
      bookmark_.reset();
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bookmark is empty");
    }
  }

  context.getProperty(MaxBufferSize, max_buffer_size_);
  logger_->log_debug("ConsumeWindowsEventLog: MaxBufferSize {}", max_buffer_size_);

  context.getProperty(CacheSidLookups, cache_sid_lookups_);
  logger_->log_debug("ConsumeWindowsEventLog: will{} cache SID to name lookups", cache_sid_lookups_ ? "" : " not");

  provenanceUri_ = "winlog://" + computerName_ + "/" + path_.str() + "?" + query;
  logger_->log_trace("Successfully configured CWEL");
}

bool ConsumeWindowsEventLog::commitAndSaveBookmark(const std::wstring &bookmark_xml, core::ProcessContext& context, core::ProcessSession& session) {
  {
    const TimeDiff time_diff;
    session.commit();
    context.getStateManager()->beginTransaction();
    logger_->log_debug("processQueue commit took {}", time_diff());
  }

  if (!bookmark_->saveBookmarkXml(bookmark_xml)) {
    logger_->log_error("Failed to save bookmark xml");
  }

  if (session.outgoingConnectionsFull("success")) {
    logger_->log_debug("Outgoing success connection is full");
    return false;
  }

  return true;
}

std::tuple<size_t, std::wstring> ConsumeWindowsEventLog::processEventLogs(core::ProcessSession& session, const EVT_HANDLE& event_query_results) {
  size_t processed_event_count = 0;
  std::wstring bookmark_xml;
  logger_->log_trace("Enumerating the events in the result set after the bookmarked event.");
  while (processed_event_count < batch_commit_size_ || batch_commit_size_ == 0) {
    EVT_HANDLE next_event{};
    DWORD handles_set_count{};
    if (!EvtNext(event_query_results, 1, &next_event, EVT_NEXT_TIMEOUT_MS, 0, &handles_set_count)) {
      if (ERROR_NO_MORE_ITEMS != GetLastError()) {
        auto last_error = utils::OsUtils::windowsErrorToErrorCode(GetLastError());
        logger_->log_error("Failed to get next event: {}: {}", last_error, last_error.message());
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
      logger_->log_error("{}", event_render.error());
      continue;
    }
    auto new_bookmark_xml = bookmark_->getNewBookmarkXml(next_event);
    if (!new_bookmark_xml) {
      logger_->log_error("{}", new_bookmark_xml.error());
      continue;
    }
    bookmark_xml = std::move(*new_bookmark_xml);
    processed_event_count++;
    putEventRenderFlowFileToSession(*event_render, session);
  }
  logger_->log_trace("Finished enumerating events.");
  return std::make_tuple(processed_event_count, bookmark_xml);
}

void ConsumeWindowsEventLog::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  std::unique_lock<std::mutex> lock(on_trigger_mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
    return;
  }

  if (!bookmark_) {
    logger_->log_debug("bookmark_ is null");
    context.yield();
    return;
  }

  logger_->log_trace("CWEL onTrigger");

  size_t processed_event_count = 0;
  const TimeDiff time_diff;
  const auto timeGuard = gsl::finally([&]() {
    logger_->log_debug("processed {} Events in {}", processed_event_count, time_diff());
  });

  wel::unique_evt_handle event_query_results{EvtQuery(nullptr, path_.wstr().c_str(), wstr_query_.c_str(), path_.getQueryFlags())};
  if (!event_query_results) {
    LOG_LAST_ERROR(EvtQuery);
    context.yield();
    return;
  }

  logger_->log_trace("Retrieved results in Channel: {} with Query: {}", utils::to_string(path_.wstr()), utils::to_string(wstr_query_));

  auto bookmark_handle = bookmark_->getBookmarkHandleFromXML();
  if (!bookmark_handle) {
    logger_->log_error("bookmark_handle is null, unrecoverable error!");
    bookmark_.reset();
    context.yield();
    return;
  }

  if (!EvtSeek(event_query_results.get(), 1, bookmark_handle, 0, EvtSeekRelativeToBookmark)) {
    LOG_LAST_ERROR(EvtSeek);
    context.yield();
    return;
  }

  refreshTimeZoneData();

  std::wstring bookmark_xml;
  std::tie(processed_event_count, bookmark_xml) = processEventLogs(session, event_query_results.get());

  if (processed_event_count == 0 || !commitAndSaveBookmark(bookmark_xml, context, session)) {
    context.yield();
    return;
  }
}

wel::WindowsEventLogHandler& ConsumeWindowsEventLog::getEventLogHandler(const std::string & name) {
  logger_->log_trace("Getting Event Log Handler corresponding to {}", name.c_str());
  auto provider = providers_.find(name);
  if (provider != std::end(providers_)) {
    logger_->log_trace("Found the handler");
    return provider->second;
  }

  std::wstring temp_wstring = std::wstring(name.begin(), name.end());
  LPCWSTR widechar = temp_wstring.c_str();

  auto opened_publisher_metadata_provider = EvtOpenPublisherMetadata(nullptr, widechar, nullptr, 0, 0);
  if (!opened_publisher_metadata_provider)
    logger_->log_warn("EvtOpenPublisherMetadata failed due to {}", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message());
  providers_[name] = wel::WindowsEventLogHandler(opened_publisher_metadata_provider);
  logger_->log_info("Handler not found for {}, creating. Number of cached handlers: {}", name, providers_.size());
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

            value = utils::string::trimRight(value);

            xmlPercentageItemsResolutions_.insert({key, value});
          } else {
            // Add "" to xmlPercentageItemsResolutions_ - don't need to call FormaMessage for this 'key' again.
            xmlPercentageItemsResolutions_.insert({key, ""});

            logger_->log_error("!FormatMessage error: {:#x}. '{}' is not found in msobjs.dll.", GetLastError(), key.c_str());
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
      std::string error_message = fmt::format("EvtRender failed due to {}", utils::OsUtils::windowsErrorToErrorCode(last_error).message());
      return nonstd::make_unexpected(error_message);
    }
    if (used > max_buffer_size_) {
      std::string error_message = fmt::format("Dropping event because it couldn't be rendered within {} bytes.", max_buffer_size_);
      return nonstd::make_unexpected(error_message);
    }
    size = used;
    buf.reset((LPWSTR) malloc(size));
    if (!buf)
      return nonstd::make_unexpected("malloc failed");
    if (!EvtRender(nullptr, event_handle, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
      std::string error_message = fmt::format("EvtRender failed due to {}", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message());
      return nonstd::make_unexpected(error_message);
    }
  }
  logger_->log_trace("Event rendered with size {}", used);
  return utils::to_string(std::wstring{buf.get()});
}

nonstd::expected<cwel::EventRender, std::string> ConsumeWindowsEventLog::createEventRender(EVT_HANDLE hEvent) {
  auto event_as_xml = renderEventAsXml(hEvent);
  if (!event_as_xml)
    return nonstd::make_unexpected(event_as_xml.error());

  pugi::xml_document doc;
  if (!doc.load_string(event_as_xml->c_str()))
    return nonstd::make_unexpected("Invalid XML produced");

  cwel::EventRender result;

  // this is a well known path.
  std::string provider_name = doc.child("Event").child("System").child("Provider").attribute("Name").value();
  wel::WindowsEventLogMetadataImpl metadata{getEventLogHandler(provider_name).getMetadata(), hEvent};
  wel::MetadataWalker walker{metadata, path_.str(), !resolve_as_attributes_, apply_identifier_function_, regex_ ? &*regex_ : nullptr, userIdToUsernameFunction()};

  // resolve the event metadata
  doc.traverse(walker);

  logger_->log_trace("Finish doc traversing, performing writing...");

  if (output_format_ == cwel::OutputFormat::Plaintext || output_format_ == cwel::OutputFormat::Both) {
    logger_->log_trace("Writing event in plain text");

    auto& handler = getEventLogHandler(provider_name);
    auto event_message = handler.getEventMessage(hEvent);

    if (event_message) {
      for (const auto& map_entry : walker.getIdentifiers()) {
        if (map_entry.first.empty() || map_entry.second.empty()) {
          continue;
        }
        utils::string::replaceAll(*event_message, map_entry.first, map_entry.second);
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

  if (output_format_ != cwel::OutputFormat::Plaintext) {
    substituteXMLPercentageItems(doc);
    logger_->log_trace("Finish substituting %% in XML");
  }

  if (resolve_as_attributes_) {
    result.matched_fields = walker.getFieldValues();
  }

  if (output_format_ == cwel::OutputFormat::XML || output_format_ == cwel::OutputFormat::Both) {
    logger_->log_trace("Writing event in XML");

    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    event_as_xml = writer.xml_;

    result.xml = std::move(*event_as_xml);
    logger_->log_trace("Finish writing in XML");
  }

  if (output_format_ == cwel::OutputFormat::JSON) {
    switch (json_format_) {
      case cwel::JsonFormat::Raw: {
        logger_->log_trace("Writing event in raw JSON");
        result.json = wel::jsonToString(wel::toRawJSON(doc));
        logger_->log_trace("Finish writing in raw JSON");
        break;
      }
      case cwel::JsonFormat::Simple: {
        logger_->log_trace("Writing event in simple JSON");
        result.json = wel::jsonToString(wel::toSimpleJSON(doc));
        logger_->log_trace("Finish writing in simple JSON");
        break;
      }
      case cwel::JsonFormat::Flattened: {
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

  timezone_name_ = utils::to_string(tzstr);
  timezone_offset_ = tzoffset.str();

  logger_->log_trace("Timezone name: {}, offset: {}", timezone_name_, timezone_offset_);
}

void ConsumeWindowsEventLog::putEventRenderFlowFileToSession(const cwel::EventRender& eventRender, core::ProcessSession& session) const {
  auto commitFlowFile = [&] (const std::string& content, const std::string& mimeType) {
    auto flow_file = session.create();
    addMatchedFieldsAsAttributes(eventRender, session, flow_file);
    session.writeBuffer(flow_file, content);
    session.putAttribute(*flow_file, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
    session.putAttribute(*flow_file, "timezone.name", timezone_name_);
    session.putAttribute(*flow_file, "timezone.offset", timezone_offset_);
    session.getProvenanceReporter()->receive(*flow_file, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0ms);
    session.transfer(flow_file, Success);
  };

  if (output_format_ == cwel::OutputFormat::XML || output_format_ == cwel::OutputFormat::Both) {
    logger_->log_trace("Writing rendered XML to a flow file");
    commitFlowFile(eventRender.xml, "application/xml");
  }

  if (output_format_ == cwel::OutputFormat::Plaintext || output_format_ == cwel::OutputFormat::Both) {
    logger_->log_trace("Writing rendered plain text to a flow file");
    commitFlowFile(eventRender.plaintext, "text/plain");
  }

  if (output_format_ == cwel::OutputFormat::JSON) {
    logger_->log_trace("Writing rendered {} JSON to a flow file", magic_enum::enum_name(json_format_));
    commitFlowFile(eventRender.json, "application/json");
  }
}

void ConsumeWindowsEventLog::addMatchedFieldsAsAttributes(const cwel::EventRender& eventRender, core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flowFile) const {
  for (const auto &fieldMapping : eventRender.matched_fields) {
    if (!fieldMapping.second.empty()) {
      session.putAttribute(*flowFile, fieldMapping.first, fieldMapping.second);
    }
  }
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
