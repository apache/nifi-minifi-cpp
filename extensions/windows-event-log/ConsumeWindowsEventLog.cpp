/**
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
#include <map>
#include <string>
#include <memory>

#include "wel/Bookmark.h"
#include "wel/LookupCacher.h"
#include "wel/MetadataWalker.h"
#include "wel/StringSplitter.h"
#include "wel/XMLString.h"
#include "wel/JSONUtils.h"
#include "wel/WindowsError.h"

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/Deleters.h"
#include "core/logging/LoggerFactory.h"

#include "minifi-cpp/utils/gsl.h"
#include "utils/RegexUtils.h"
#include "utils/StringUtils.h"
#include "utils/UnicodeConversion.h"
#include "utils/ProcessorConfigUtils.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace std::literals::chrono_literals;

namespace {
auto createTimer() {
  return [start_time = std::chrono::steady_clock::now()] {
    return std::chrono::steady_clock::now() - start_time;
  };
}
}  // namespace

namespace org::apache::nifi::minifi::wel {
std::function<bool(std::string_view)> parseSidMatcher(const std::optional<std::string>& sid_matcher) {
  if (!sid_matcher || sid_matcher->empty()) {
    return [](std::string_view){ return false; };
  }

  if (std::smatch match; utils::regexMatch(*sid_matcher, match, utils::Regex{R"_(\.\*(\w+))_"})) {
    std::string suffix = match[1];
    return [suffix](std::string_view field_name) { return utils::string::endsWith(field_name, suffix); };
  }

  utils::Regex sid_matcher_regex{*sid_matcher};
  return [sid_matcher_regex](std::string_view field_name) { return utils::regexMatch(field_name, sid_matcher_regex); };
}
}  // namespace org::apache::nifi::minifi::wel

namespace org::apache::nifi::minifi::processors {

ConsumeWindowsEventLog::ConsumeWindowsEventLog(core::ProcessorMetadata metadata)
    : core::ProcessorImpl{std::move(metadata)} {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    logger_->log_error("GetComputerName failed due to {}", wel::getLastError());
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
      logger_->log_error("FreeLibrary failed due to {}", wel::getLastError());
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

wel::HeaderNames ConsumeWindowsEventLog::createHeaderNames(const std::optional<std::string>& event_header_property) const {
  if (!event_header_property) { return {}; }

  wel::HeaderNames header_names;
  wel::splitCommaSeparatedKeyValuePairs(*event_header_property, [this, &header_names](std::string_view key, std::string_view value) {
    if (auto metadata = magic_enum::enum_cast<wel::Metadata>(key)) {
      header_names.emplace_back(*metadata, value);
    } else {
      logger_->log_error("{} is an invalid key for the header map", key);
    }
  });
  return header_names;
}

void ConsumeWindowsEventLog::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  resolve_as_attributes_ = utils::parseBoolProperty(context, ResolveAsAttributes);
  apply_identifier_function_ = utils::parseBoolProperty(context, IdentifierFunction);
  header_delimiter_ = utils::parseOptionalProperty(context, EventHeaderDelimiter);
  batch_commit_size_ = utils::parseU64Property(context, BatchCommitSize);
  header_names_ = createHeaderNames(utils::parseOptionalProperty(context, EventHeader));
  sid_matcher_ = wel::parseSidMatcher(utils::parseOptionalProperty(context, IdentifierMatcher));
  output_format_ = utils::parseEnumProperty<wel::OutputFormat>(context, OutputFormatProperty);
  json_format_ = utils::parseEnumProperty<wel::JsonFormat>(context, JsonFormatProperty);

  if (output_format_ != wel::OutputFormat::Plaintext && !hMsobjsDll_) {
    char systemDir[MAX_PATH];
    if (GetSystemDirectory(systemDir, sizeof(systemDir))) {
      hMsobjsDll_ = LoadLibrary((systemDir + std::string("\\msobjs.dll")).c_str());
      if (!hMsobjsDll_) {
        logger_->log_error("LoadLibrary failed due to {}", wel::getLastError());
      }
    } else {
      logger_->log_error("GetSystemDirectory failed due to {}", wel::getLastError());
    }
  }

  path_ = wel::EventPath{utils::parseProperty(context, Channel)};
  if (path_.kind() == wel::EventPath::Kind::FILE) {
    logger_->log_debug("Using saved log file as log source");
  } else {
    logger_->log_debug("Using channel as log source");
  }

  std::string query = context.getProperty(Query).value_or("");
  wstr_query_ = utils::to_wstring(query);

  if (!bookmark_) {
    bookmark_ = createBookmark(context);
  }

  max_buffer_size_ = utils::parseDataSizeProperty(context, MaxBufferSize);
  logger_->log_debug("ConsumeWindowsEventLog: MaxBufferSize {}", max_buffer_size_);

  cache_sid_lookups_ = utils::parseBoolProperty(context, CacheSidLookups);
  logger_->log_debug("ConsumeWindowsEventLog: will{} cache SID to name lookups", cache_sid_lookups_ ? "" : " not");

  provenanceUri_ = "winlog://" + computerName_ + "/" + path_.str() + "?" + query;
  logger_->log_trace("Successfully configured CWEL");
}

std::unique_ptr<wel::Bookmark> ConsumeWindowsEventLog::createBookmark(const core::ProcessContext& context) const {
  std::string bookmark_dir = context.getProperty(BookmarkRootDirectory).value_or("");
  if (bookmark_dir.empty()) {
    logger_->log_error("State Directory is empty");
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "State Directory is empty");
  }
  bool process_old_events = utils::parseBoolProperty(context, ProcessOldEvents);
  auto bookmark = std::make_unique<wel::Bookmark>(path_, wstr_query_, bookmark_dir, getUUID(), process_old_events, state_manager_, logger_);
  if (!bookmark->isValid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bookmark is empty");
  }
  return bookmark;
}

bool ConsumeWindowsEventLog::commitAndSaveBookmark(const std::wstring &bookmark_xml, core::ProcessContext& context, core::ProcessSession& session) {
  {
    const auto time_diff = createTimer();
    session.commit();
    context.getStateManager()->beginTransaction();
    logger_->log_debug("ConsumeWindowsEventLog: commit took {}", time_diff());
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

std::tuple<size_t, std::wstring> ConsumeWindowsEventLog::processEventLogs(core::ProcessSession& session, EVT_HANDLE event_query_results) {
  static constexpr DWORD timeout_milliseconds = 500;
  size_t processed_event_count = 0;
  std::wstring bookmark_xml;
  logger_->log_trace("Enumerating the events in the result set after the bookmarked event.");
  while (processed_event_count < batch_commit_size_ || batch_commit_size_ == 0) {
    EVT_HANDLE next_event_handle{};
    DWORD handles_set_count{};
    if (!EvtNext(event_query_results, 1, &next_event_handle, timeout_milliseconds, 0, &handles_set_count)) {
      if (ERROR_NO_MORE_ITEMS != GetLastError()) {
        logger_->log_error("Failed to get next event due to {}", wel::getLastError());
        continue;
        /* According to MS this iteration should only end when the return value is false AND
          the error code is NO_MORE_ITEMS. See the following page for further details:
          https://docs.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtnext */
      }
      break;
    }
    wel::unique_evt_handle next_event{next_event_handle};
    logger_->log_trace("Successfully got the next event, performing event rendering");
    auto processed_event = processEvent(next_event.get());
    if (!processed_event) {
      logger_->log_error("error processing event: {}", processed_event.error());
      continue;
    }
    auto new_bookmark_xml = bookmark_->getNewBookmarkXml(next_event.get());
    if (!new_bookmark_xml) {
      logger_->log_error("error getting the new bookmark: {}", new_bookmark_xml.error());
      continue;
    }
    bookmark_xml = std::move(*new_bookmark_xml);
    processed_event_count++;
    createAndCommitFlowFile(*processed_event, session);
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
  const auto time_diff = createTimer();
  const auto timeGuard = gsl::finally([&]() {
    logger_->log_debug("processed {} Events in {}", processed_event_count, time_diff());
  });

  wel::unique_evt_handle event_query_results{EvtQuery(nullptr, path_.wstr().c_str(), wstr_query_.c_str(), path_.getQueryFlags())};
  if (!event_query_results) {
    logger_->log_error("EvtQuery failed due to {}", wel::getLastError());
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
    logger_->log_error("EvtSeek failed due to {}", wel::getLastError());
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

wel::WindowsEventLogProvider& ConsumeWindowsEventLog::getEventLogProvider(const std::string& name) {
  logger_->log_trace("Getting Event Log Provider corresponding to {}", name);
  auto it = providers_.find(name);
  if (it != std::end(providers_)) {
    logger_->log_trace("Found cached event log provider");
    return it->second;
  }

  std::wstring temp_wstring = utils::to_wstring(name);
  LPCWSTR widechar = temp_wstring.c_str();

  auto opened_publisher_metadata_provider = EvtOpenPublisherMetadata(nullptr, widechar, nullptr, 0, 0);
  if (!opened_publisher_metadata_provider) {
    logger_->log_warn("EvtOpenPublisherMetadata failed due to {}", wel::getLastError());
  }
  providers_.emplace(name, opened_publisher_metadata_provider);
  logger_->log_info("Created new Windows event log provider for '{}'. Number of cached providers: {}", name, providers_.size());
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

            logger_->log_error("FormatMessage failed due to {}. '{}' is not found in msobjs.dll.", wel::getLastError(), key);
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
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
      return nonstd::make_unexpected(fmt::format("EvtRender failed due to {}", wel::getLastError()));
    }
    if (used > max_buffer_size_) {
      return nonstd::make_unexpected(fmt::format("Dropping event because it couldn't be rendered within {} bytes.", max_buffer_size_));
    }
    size = used;
    buf.reset((LPWSTR) malloc(size));
    if (!buf) {
      return nonstd::make_unexpected("malloc failed");
    }
    if (!EvtRender(nullptr, event_handle, EvtRenderEventXml, size, buf.get(), &used, &propertyCount)) {
      return nonstd::make_unexpected(fmt::format("EvtRender failed due to {}", wel::getLastError()));
    }
  }
  logger_->log_trace("Event rendered with size {}", used);
  return utils::to_string(std::wstring{buf.get()});
}

nonstd::expected<wel::ProcessedEvent, std::string> ConsumeWindowsEventLog::processEvent(EVT_HANDLE event_handle) {
  auto event_as_xml = renderEventAsXml(event_handle);
  if (!event_as_xml)
    return nonstd::make_unexpected(event_as_xml.error());

  pugi::xml_document doc;
  if (!doc.load_string(event_as_xml->c_str()))
    return nonstd::make_unexpected("Invalid XML produced");

  wel::ProcessedEvent result;

  // this is a well known path.
  std::string provider_name = doc.child("Event").child("System").child("Provider").attribute("Name").value();
  wel::WindowsEventLogMetadataImpl metadata{getEventLogProvider(provider_name), event_handle};
  wel::MetadataWalker walker{metadata, path_.str(), !resolve_as_attributes_, apply_identifier_function_, sid_matcher_, userIdToUsernameFunction()};

  // resolve the event metadata
  doc.traverse(walker);

  logger_->log_trace("Finish doc traversing, performing writing...");

  if (output_format_ == wel::OutputFormat::Plaintext || output_format_ == wel::OutputFormat::Both) {
    logger_->log_trace("Writing event in plain text");

    auto& provider = getEventLogProvider(provider_name);
    auto event_message = provider.getEventMessage(event_handle);

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
    result.plaintext = log_header.getEventHeader([&walker](wel::Metadata metadata) { return walker.getMetadata(metadata); });
    result.plaintext += payload_name;
    result.plaintext += log_header.getDelimiterFor(payload_name.size());
    result.plaintext += event_message.has_value() ? *event_message : event_message.error().message();

    logger_->log_trace("Finish writing in plain text");
  }

  if (output_format_ != wel::OutputFormat::Plaintext) {
    substituteXMLPercentageItems(doc);
    logger_->log_trace("Finish substituting %% in XML");
  }

  if (resolve_as_attributes_) {
    result.matched_fields = walker.getFieldValues();
  }

  if (output_format_ == wel::OutputFormat::XML || output_format_ == wel::OutputFormat::Both) {
    logger_->log_trace("Writing event in XML");

    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    event_as_xml = writer.xml_;

    result.xml = std::move(*event_as_xml);
    logger_->log_trace("Finish writing in XML");
  }

  if (output_format_ == wel::OutputFormat::JSON) {
    switch (json_format_) {
      case wel::JsonFormat::Raw: {
        logger_->log_trace("Writing event in raw JSON");
        result.json = wel::jsonToString(wel::toRawJSON(doc));
        logger_->log_trace("Finish writing in raw JSON");
        break;
      }
      case wel::JsonFormat::Simple: {
        logger_->log_trace("Writing event in simple JSON");
        result.json = wel::jsonToString(wel::toSimpleJSON(doc));
        logger_->log_trace("Finish writing in simple JSON");
        break;
      }
      case wel::JsonFormat::Flattened: {
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

  timezone_name_ = utils::to_string(tzstr);

  tzbias *= -1;  // WinApi specifies UTC = localtime + bias, but we need offset from UTC
  timezone_offset_ = fmt::format("{}{:02}:{:02}", tzbias >= 0 ? '+' : '-', std::abs(tzbias) / 60, std::abs(tzbias) % 60);

  logger_->log_trace("Timezone name: {}, offset: {}", timezone_name_, timezone_offset_);
}

void ConsumeWindowsEventLog::createAndCommitFlowFile(const wel::ProcessedEvent& processed_event, core::ProcessSession& session) const {
  auto commit_flow_file = [&] (const std::string& content, const std::string& mimeType) {
    auto flow_file = session.create();
    for (const auto& [key, value] : processed_event.matched_fields) {
      if (!value.empty()) {
        session.putAttribute(*flow_file, key, value);
      }
    }
    session.writeBuffer(flow_file, content);
    session.putAttribute(*flow_file, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
    session.putAttribute(*flow_file, "timezone.name", timezone_name_);
    session.putAttribute(*flow_file, "timezone.offset", timezone_offset_);
    session.getProvenanceReporter()->receive(*flow_file, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0ms);
    session.transfer(flow_file, Success);
  };

  if (output_format_ == wel::OutputFormat::XML || output_format_ == wel::OutputFormat::Both) {
    logger_->log_trace("Writing rendered XML to a flow file");
    commit_flow_file(processed_event.xml, "application/xml");
  }

  if (output_format_ == wel::OutputFormat::Plaintext || output_format_ == wel::OutputFormat::Both) {
    logger_->log_trace("Writing rendered plain text to a flow file");
    commit_flow_file(processed_event.plaintext, "text/plain");
  }

  if (output_format_ == wel::OutputFormat::JSON) {
    logger_->log_trace("Writing rendered {} JSON to a flow file", magic_enum::enum_name(json_format_));
    commit_flow_file(processed_event.json, "application/json");
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
