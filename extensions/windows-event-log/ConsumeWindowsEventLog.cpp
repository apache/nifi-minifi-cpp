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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <memory>
#include <regex>
#include <cinttypes>

#include "wel/MetadataWalker.h"
#include "wel/XMLString.h"
#include "wel/UnicodeConversion.h"

#include "utils/ScopeGuard.h"
#include "io/DataStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Bookmark.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// ConsumeWindowsEventLog
const std::string ConsumeWindowsEventLog::ProcessorName("ConsumeWindowsEventLog");

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
  withDefaultValue("LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType")->
  withDescription("Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.")->
  build());

core::Property ConsumeWindowsEventLog::OutputFormat(
  core::PropertyBuilder::createProperty("Output Format")->
  isRequired(true)->
  withDefaultValue(Both)->
  withAllowableValues<std::string>({XML, Plaintext, Both})->
  withDescription("Set the output format type. In case \'Both\' is selected the processor generates two flow files for every event captured")->
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

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger()), apply_identifier_function_(false), batch_commit_size_(0U) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LogWindowsError();
  }

  writeXML_ = false;
  writePlainText_ = false;
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
     EventHeaderDelimiter, EventHeader, OutputFormat, BatchCommitSize, BookmarkRootDirectory, ProcessOldEvents
  });

  //! Set the supported relationships
  setSupportedRelationships({Success});
}

bool ConsumeWindowsEventLog::insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string & value) {

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

  writeXML_ = (mode == Both || mode == XML);

  writePlainText_ = (mode == Both || mode == Plaintext);

  if (writeXML_ && !hMsobjsDll_) {
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

  if (!pBookmark_) {
    std::string bookmarkDir;
    context->getProperty(BookmarkRootDirectory.getName(), bookmarkDir);
    if (bookmarkDir.empty()) {
      logger_->log_error("State Directory is empty");
      return;
    }
    pBookmark_ = std::make_unique<Bookmark>(wstrChannel_, wstrQuery_, bookmarkDir, getUUIDStr(), processOldEvents, state_manager_, logger_);
    if (!*pBookmark_) {
      pBookmark_.reset();
      return;
    }
  }

  context->getProperty(MaxBufferSize.getName(), maxBufferSize_);
  logger_->log_debug("ConsumeWindowsEventLog: maxBufferSize_ %" PRIu64, maxBufferSize_);

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel_ + "?" + query;
}


void ConsumeWindowsEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (!pBookmark_) {
    context->yield();
    return;
  }

  std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
    return;
  }

  struct TimeDiff {
    auto operator()() const {
      return int64_t{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time_).count() };
    }
    const decltype(std::chrono::steady_clock::now()) time_ = std::chrono::steady_clock::now();
  };

  const auto commitAndSaveBookmark = [&] (const std::wstring& bookmarkXml) {
    const TimeDiff timeDiff;
    session->commit();
    logger_->log_debug("processQueue commit took %" PRId64 " ms", timeDiff());

    pBookmark_->saveBookmarkXml(bookmarkXml);

    if (session->outgoingConnectionsFull("success")) {
      logger_->log_debug("outgoingConnectionsFull");
      return false;
    }

    return true;
  };

  size_t eventCount = 0;
  const TimeDiff timeDiff;
  utils::ScopeGuard timeGuard([&]() {
    logger_->log_debug("processed %zu Events in %"  PRId64 " ms", eventCount, timeDiff());
  });

  size_t commitAndSaveBookmarkCount = 0;
  std::wstring bookmarkXml;

  const auto hEventResults = EvtQuery(0, wstrChannel_.c_str(), wstrQuery_.c_str(), EvtQueryChannelPath);
  if (!hEventResults) {
    LOG_LAST_ERROR(EvtQuery);
    return;
  }
  const utils::ScopeGuard guard_hEventResults([hEventResults]() { EvtClose(hEventResults); });

  auto hBookmark = pBookmark_->getBookmarkHandleFromXML();
  if (!hBookmark) {
    // Unrecovarable error.
    pBookmark_.reset();
    return;
  }

  if (!EvtSeek(hEventResults, 1, hBookmark, 0, EvtSeekRelativeToBookmark)) {
    LOG_LAST_ERROR(EvtSeek);
    return;
  }

  // Enumerate the events in the result set after the bookmarked event.
  while (true) {
    EVT_HANDLE hEvent{};
    DWORD dwReturned{};
    if (!EvtNext(hEventResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
      if (ERROR_NO_MORE_ITEMS != GetLastError()) {
        LOG_LAST_ERROR(EvtNext);
      }
      break;
    }
    const utils::ScopeGuard guard_hEvent([hEvent]() { EvtClose(hEvent); });

    EventRender eventRender;
    std::wstring newBookmarkXml;
    if (createEventRender(hEvent, eventRender) && pBookmark_->getNewBookmarkXml(hEvent, newBookmarkXml)) {
      bookmarkXml = std::move(newBookmarkXml);
      eventCount++;
      putEventRenderFlowFileToSession(eventRender, *session);

      if (batch_commit_size_ != 0U && (eventCount % batch_commit_size_ == 0)) {
        if (!commitAndSaveBookmark(bookmarkXml)) {
          return;
        }

        commitAndSaveBookmarkCount = eventCount;
      }
    }
  }

  if (eventCount > commitAndSaveBookmarkCount) {
    commitAndSaveBookmark(bookmarkXml);
  }
}

wel::WindowsEventLogHandler ConsumeWindowsEventLog::getEventLogHandler(const std::string & name) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  auto provider = providers_.find(name);
  if (provider != std::end(providers_)) {
    return provider->second;
  }

  std::wstring temp_wstring = std::wstring(name.begin(), name.end());
  LPCWSTR widechar = temp_wstring.c_str();

  providers_[name] = wel::WindowsEventLogHandler(EvtOpenPublisherMetadata(NULL, widechar, NULL, 0, 0));

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
        } catch (std::invalid_argument& e) {
          continue;
        }

        const std::string key = std::to_string(number);

        std::string value;
        const auto it = xmlPercentageItemsResolutions_.find(key);
        if (it == xmlPercentageItemsResolutions_.end()) {
          LPTSTR pBuffer{};
          if (FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
            hMsobjsDll_,
            number,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&pBuffer,
            1024,
            0
          )) {
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
  DWORD size = 0;
  DWORD used = 0;
  DWORD propertyCount = 0;
  EvtRender(NULL, hEvent, EvtRenderEventXml, size, 0, &used, &propertyCount);
  if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
    LOG_LAST_ERROR(EvtRender);
    return false;
  }

  if (used > maxBufferSize_) {
    logger_->log_error("Dropping event because it couldn't be rendered within %" PRIu64 " bytes.", maxBufferSize_);
    return false;
  }

  size = used;
  std::vector<wchar_t> buf(size / 2 + 1);
  if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
    LOG_LAST_ERROR(EvtRender);
    return false;
  }

  std::string xml = wel::to_string(&buf[0]);

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (!result) {
    logger_->log_error("Invalid XML produced");
    return false;
  }

  // this is a well known path. 
  std::string providerName = doc.child("Event").child("System").child("Provider").attribute("Name").value();
  wel::MetadataWalker walker(getEventLogHandler(providerName).getMetadata(), channel_, hEvent, !resolve_as_attributes_, apply_identifier_function_, regex_);

  // resolve the event metadata
  doc.traverse(walker);

  if (writePlainText_) {
    auto handler = getEventLogHandler(providerName);
    auto message = handler.getEventMessage(hEvent);

    if (!message.empty()) {

      for (const auto &mapEntry : walker.getIdentifiers()) {
        // replace the identifiers with their translated strings.
        utils::StringUtils::replaceAll(message, mapEntry.first, mapEntry.second);
      }
      wel::WindowsEventLogHeader log_header(header_names_);
      // set the delimiter
      log_header.setDelimiter(header_delimiter_);
      // render the header.
      eventRender.rendered_text_ = log_header.getEventHeader(&walker);
      eventRender.rendered_text_ += "Message" + header_delimiter_ + " ";
      eventRender.rendered_text_ += message;
    }
  }

  if (writeXML_) {
    substituteXMLPercentageItems(doc);

    if (resolve_as_attributes_) {
      eventRender.matched_fields_ = walker.getFieldValues();
    }

    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw); // no indentation or formatting
    xml = writer.xml_;

    eventRender.text_ = std::move(xml);
  }

  return true;
}

void ConsumeWindowsEventLog::putEventRenderFlowFileToSession(const EventRender& eventRender, core::ProcessSession& session)
{
  struct WriteCallback : public OutputStreamCallback {
    WriteCallback(const std::string& str)
      : str_(str) {
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      return stream->writeData(reinterpret_cast<uint8_t*>(const_cast<char*>(str_.c_str())), str_.size());
    }

    const std::string& str_;
  };

  if (writeXML_) {
    auto flowFile = session.create();

    session.write(flowFile, &WriteCallback(eventRender.text_));
    for (const auto &fieldMapping : eventRender.matched_fields_) {
      if (!fieldMapping.second.empty()) {
        session.putAttribute(flowFile, fieldMapping.first, fieldMapping.second);
      }
    }
    session.putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "application/xml");
    session.getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session.transfer(flowFile, Success);
  }

  if (writePlainText_) {
    auto flowFile = session.create();

    session.write(flowFile, &WriteCallback(eventRender.rendered_text_));
    session.putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
    session.getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session.transfer(flowFile, Success);
  }
}

void ConsumeWindowsEventLog::notifyStop() {
  state_manager_->persist();
  state_manager_.reset();
}

void ConsumeWindowsEventLog::LogWindowsError()
{
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

  logger_->log_error("Error %x: %s\n", (int)error_id, (char *)lpMsg);

  LocalFree(lpMsg);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
