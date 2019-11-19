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
  withDescription("Directory which contains processor state data.")->
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
}

void ConsumeWindowsEventLog::initialize() {
  //! Set the supported properties
  setSupportedProperties(
    {Channel, Query, MaxBufferSize, InactiveDurationToReconnect, IdentifierMatcher, IdentifierFunction, ResolveAsAttributes, EventHeaderDelimiter, EventHeader, OutputFormat, BatchCommitSize, BookmarkRootDirectory}
  );

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
  context->getProperty(IdentifierMatcher.getName(), regex_);
  context->getProperty(ResolveAsAttributes.getName(), resolve_as_attributes_);
  context->getProperty(IdentifierFunction.getName(), apply_identifier_function_);
  context->getProperty(EventHeaderDelimiter.getName(), header_delimiter_);
  context->getProperty(BatchCommitSize.getName(), batch_commit_size_);

  std::string bookmarkDir;
  context->getProperty(BookmarkRootDirectory.getName(), bookmarkDir);
  if (bookmarkDir.empty()) {
    logger_->log_error("State Directory is empty");
  } else {
    pBookmark_ = std::make_unique<Bookmark>(bookmarkDir, getUUIDStr(), logger_);
    if (!*pBookmark_) {
      pBookmark_.reset();
    }
  }

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
    }
    else if (splitKeyAndValue.size() == 1) {
     auto key = utils::StringUtils::trim(splitKeyAndValue.at(0));
     if (!insertHeaderName(header_names_, key, "")) {
       logger_->log_error("%s is an invalid key for the header map", key);
     }
    }
  }

  if (subscriptionHandle_) {
    logger_->log_error("Processor already subscribed to Event Log, expected cleanup to unsubscribe.");
  } else {
    sessionFactory_ = sessionFactory;

    subscribe(context);
  }

  std::string mode;
  context->getProperty(OutputFormat.getName(), mode);

  writeXML_ = (mode == Both || mode == XML);

  writePlainText_ = (mode == Both || mode == Plaintext);
}

void ConsumeWindowsEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (!subscriptionHandle_) {
    if (!subscribe(context)) {
      context->yield();
      return;
    }
  }

  std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
    return;
  }

  const auto flowFileCount = processQueue(session);

  const auto now = GetTickCount64();

  if (flowFileCount > 0) {
    lastActivityTimestamp_ = now;
  }
  else if (inactiveDurationToReconnect_ > 0) {
    if ((now - lastActivityTimestamp_) > inactiveDurationToReconnect_) {
      logger_->log_info("Exceeds configured 'inactive duration to reconnect' %lld ms. Unsubscribe to reconnect..", inactiveDurationToReconnect_);
      unsubscribe();
    }
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

void ConsumeWindowsEventLog::processEvent(EVT_HANDLE hEvent) {
  DWORD size = 0;
  DWORD used = 0;
  DWORD propertyCount = 0;
  if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, 0, &used, &propertyCount)) {
    if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
      if (used > maxBufferSize_) {
        logger_->log_error("Dropping event %x because it couldn't be rendered within %ll bytes.", hEvent, maxBufferSize_);
        return;
      }

      size = used;
      std::vector<wchar_t> buf(size / 2 + 1);
      if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
        logger_->log_error("!EvtRender error: %d.", GetLastError());
        return;
      }

      std::string xml = wel::to_string(&buf[0]);

      pugi::xml_document doc;
      pugi::xml_parse_result result = doc.load_string(xml.c_str());

      if (!result) {
        logger_->log_error("Invalid XML produced");
        return;
      }
      // this is a well known path. 
      std::string providerName = doc.child("Event").child("System").child("Provider").attribute("Name").value();
      wel::MetadataWalker walker(getEventLogHandler(providerName).getMetadata(), channel_, hEvent, !resolve_as_attributes_, apply_identifier_function_, regex_);

      // resolve the event metadata
      doc.traverse(walker);

      EventRender renderedData;

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
          renderedData.rendered_text_ = log_header.getEventHeader(&walker);
          renderedData.rendered_text_ += "Message" + header_delimiter_ + " ";
          renderedData.rendered_text_ += message;
        }
      }

      if (writeXML_) {
        if (resolve_as_attributes_) {
          renderedData.matched_fields_ = walker.getFieldValues();
        }

        wel::XmlString writer;
        doc.print(writer, "", pugi::format_raw); // no indentation or formatting
        xml = writer.xml_;

        renderedData.text_ = std::move(xml);
      }

      if (pBookmark_) {
        std::wstring bookmarkXml;
        if (pBookmark_->getNewBookmarkXml(hEvent, bookmarkXml)) {
          renderedData.bookmarkXml_ = bookmarkXml;
        }
      }

      listRenderedData_.enqueue(std::move(renderedData));
    }
  }
}

bool ConsumeWindowsEventLog::processEventsAfterBookmark(EVT_HANDLE hEventResults, const std::wstring& channel, const std::wstring& query) {
  if (!EvtSeek(hEventResults, 1, pBookmark_->bookmarkHandle(), 0, EvtSeekRelativeToBookmark)) {
    logger_->log_error("!EvtSeek error %d.", GetLastError());
    return false;
  }

  // Enumerate the events in the result set after the bookmarked event.
  while (true) {
    EVT_HANDLE hEvent{};
    DWORD dwReturned{};
    if (!EvtNext(hEventResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
      DWORD status = ERROR_SUCCESS;
      if (ERROR_NO_MORE_ITEMS != (status = GetLastError())) {
        logger_->log_error("!EvtNext error %d.", status);
      }
      break;
    }

    processEvent(hEvent);

    EvtClose(hEvent);
  }

  return true;
}


bool ConsumeWindowsEventLog::subscribe(const std::shared_ptr<core::ProcessContext> &context) {
  context->getProperty(Channel.getName(), channel_);
  context->getProperty(Query.getName(), query_);

  context->getProperty(MaxBufferSize.getName(), maxBufferSize_);
  logger_->log_debug("ConsumeWindowsEventLog: maxBufferSize_ %lld", maxBufferSize_);

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel_ + "?" + query_;

  std::string strInactiveDurationToReconnect;
  context->getProperty(InactiveDurationToReconnect.getName(), strInactiveDurationToReconnect);

  // Get 'inactiveDurationToReconnect_'.
  core::TimeUnit unit;
  if (core::Property::StringToTime(strInactiveDurationToReconnect, inactiveDurationToReconnect_, unit) &&
    core::Property::ConvertTimeUnitToMS(inactiveDurationToReconnect_, unit, inactiveDurationToReconnect_)) {
    logger_->log_info("inactiveDurationToReconnect: [%lld] ms", inactiveDurationToReconnect_);
  }

  if (!pBookmark_) {
    logger_->log_error("!pBookmark_");
    return false;
  }

  auto channel = std::wstring(channel_.begin(), channel_.end());
  auto query = std::wstring(query_.begin(), query_.end());

  do {
    auto hEventResults = EvtQuery(0, channel.c_str(), query.c_str(), EvtQueryChannelPath);
    if (!hEventResults) {
      logger_->log_error("!EvtQuery error: %d.", GetLastError());
      // Consider it as a serious error.
      return false;
    }
    const utils::ScopeGuard guard_hEventResults([hEventResults]() { EvtClose(hEventResults); });

    if (pBookmark_->hasBookmarkXml()) {
      if (!processEventsAfterBookmark(hEventResults, channel, query)) {
        break;
      }
    } else {
      // Seek to the last event in the hEventResults.
      if (!EvtSeek(hEventResults, 0, 0, 0, EvtSeekRelativeToLast)) {
        logger_->log_error("!EvtSeek error: %d.", GetLastError());
        break;
      }

      DWORD dwReturned{};
      EVT_HANDLE hEvent{};
      if (!EvtNext(hEventResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
        logger_->log_error("!EvtNext error: %d.", GetLastError());
        break;
      }

      pBookmark_->saveBookmark(hEvent);
    }
  } while (false);

  subscriptionHandle_ = EvtSubscribe(
      NULL,
      NULL,
      channel.c_str(),
      query.c_str(),
      NULL,
      this,
      [](EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent)
      {
        auto pConsumeWindowsEventLog = static_cast<ConsumeWindowsEventLog*>(pContext);

        auto& logger = pConsumeWindowsEventLog->logger_;

        if (action == EvtSubscribeActionError) {
          if (ERROR_EVT_QUERY_RESULT_STALE == (DWORD)hEvent) {
            logger->log_error("Received missing event notification. Consider triggering processor more frequently or increasing queue size.");
          } else {
            logger->log_error("Received the following Win32 error: %x", hEvent);
          }
        } else if (action == EvtSubscribeActionDeliver) {
          pConsumeWindowsEventLog->processEvent(hEvent);
        }

        return 0UL;
      },
      EvtSubscribeToFutureEvents | EvtSubscribeStrict);

  if (!subscriptionHandle_) {
    logger_->log_error("Unable to subscribe with provided parameters, received the following error code: %d", GetLastError());
    return false;
  }

  lastActivityTimestamp_ = GetTickCount64();

  return true;
}

void ConsumeWindowsEventLog::unsubscribe()
{
  if (subscriptionHandle_) {
    EvtClose(subscriptionHandle_);
    subscriptionHandle_ = 0;
  }
}

int ConsumeWindowsEventLog::processQueue(const std::shared_ptr<core::ProcessSession> &session)
{
  struct WriteCallback: public OutputStreamCallback {
    WriteCallback(const std::string& str)
      : data_(str.c_str()), size_(str.length()) {
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      return stream->writeData((uint8_t*)data_, size_);
    }

    std::string str_;
    const char * data_;
    const size_t size_;
  };

  int flowFileCount = 0;

  auto before_time = std::chrono::high_resolution_clock::now();
  utils::ScopeGuard timeGuard([&](){
    logger_->log_debug("processQueue processed %d Events in %llu ms",
                      flowFileCount,
                      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - before_time).count());
  });

  bool commitAndSaveBookmark = false;

  EventRender evt;
  while (listRenderedData_.try_dequeue(evt)) {
    commitAndSaveBookmark = true;

    if (writeXML_) {
      auto flowFile = session->create();

      session->write(flowFile, &WriteCallback(evt.text_));
      for (const auto &fieldMapping : evt.matched_fields_) {
        if (!fieldMapping.second.empty()) {
          session->putAttribute(flowFile, fieldMapping.first, fieldMapping.second);
        }
      }
      session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "application/xml");
      session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs",
                                                0);
      session->transfer(flowFile, Success);
    }

    if (writePlainText_) {
      auto flowFile = session->create();

      session->write(flowFile, &WriteCallback(evt.rendered_text_));
      session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
      session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs",
                                                0);
      session->transfer(flowFile, Success);
    }

    flowFileCount++;

    if (batch_commit_size_ != 0U && (flowFileCount % batch_commit_size_ == 0)) {
      auto before_commit = std::chrono::high_resolution_clock::now();
      session->commit();
      logger_->log_debug("processQueue commit took %llu ms",
                        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - before_commit).count());

      if (pBookmark_) {
        pBookmark_->saveBookmarkXml(evt.bookmarkXml_);
      }

      commitAndSaveBookmark = false;
    }
  }

  if (commitAndSaveBookmark) {
    session->commit();

    if (pBookmark_) {
      pBookmark_->saveBookmarkXml(evt.bookmarkXml_);
    }
  }

  return flowFileCount;
}

void ConsumeWindowsEventLog::notifyStop()
{
  unsubscribe();

  if (listRenderedData_.size_approx() != 0) {
    auto session = sessionFactory_->createSession();
    if (session) {
      logger_->log_info("Finishing processing leftover events");

      processQueue(session);
    } else {
      logger_->log_error(
        "Stopping the processor but there is no ProcessSessionFactory stored and there are messages in the internal queue. "
        "Removing the processor now will clear the queue but will result in DATA LOSS. This is normally due to starting the processor, "
        "receiving events and stopping before the onTrigger happens. The messages in the internal queue cannot finish processing until "
        "the processor is triggered to run.");
    }
  }
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

  logger_->log_error("Error %d: %s\n", (int)error_id, (char *)lpMsg);

  LocalFree(lpMsg);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
