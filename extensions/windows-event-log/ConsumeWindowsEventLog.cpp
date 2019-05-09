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
#include <codecvt>

#include "io/DataStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

#pragma comment(lib, "wevtapi.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

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

core::Relationship ConsumeWindowsEventLog::Success("success", "Relationship for successfully consumed events.");

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger()) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LogWindowsError();
  }
}

void ConsumeWindowsEventLog::initialize() {
  //! Set the supported properties
  setSupportedProperties({Channel, Query, MaxBufferSize, InactiveDurationToReconnect});

  //! Set the supported relationships
  setSupportedRelationships({Success});
}

void ConsumeWindowsEventLog::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (subscriptionHandle_) {
    logger_->log_error("Processor already subscribed to Event Log, expected cleanup to unsubscribe.");
  } else {
    sessionFactory_ = sessionFactory;

    subscribe(context);
  }
}

void ConsumeWindowsEventLog::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (!subscriptionHandle_) {
    if (!subscribe(context)) {
      context->yield();
      return;
    }
  }

  const auto flowFileCount = processQueue(session);

  const auto now = GetTickCount();

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

bool ConsumeWindowsEventLog::subscribe(const std::shared_ptr<core::ProcessContext> &context)
{
  std::string channel;
  context->getProperty(Channel.getName(), channel);

  std::string query;
  context->getProperty(Query.getName(), query);

  context->getProperty(MaxBufferSize.getName(), maxBufferSize_);
  logger_->log_debug("ConsumeWindowsEventLog: maxBufferSize_ %lld", maxBufferSize_);

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel + "?" + query;

  std::string strInactiveDurationToReconnect;
  context->getProperty(InactiveDurationToReconnect.getName(), strInactiveDurationToReconnect);

  // Get 'inactiveDurationToReconnect_'.
  core::TimeUnit unit;
  if (core::Property::StringToTime(strInactiveDurationToReconnect, inactiveDurationToReconnect_, unit) &&
    core::Property::ConvertTimeUnitToMS(inactiveDurationToReconnect_, unit, inactiveDurationToReconnect_)) {
    logger_->log_info("inactiveDurationToReconnect: [%lld] ms", inactiveDurationToReconnect_);
  }

  subscriptionHandle_ = EvtSubscribe(
      NULL,
      NULL,
      std::wstring(channel.begin(), channel.end()).c_str(),
      std::wstring(query.begin(), query.end()).c_str(),
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
          DWORD size = 0;
          DWORD used = 0;
          DWORD propertyCount = 0;

          if (!EvtRender(NULL, hEvent, EvtRenderEventXml, size, 0, &used, &propertyCount)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
              if (used > pConsumeWindowsEventLog->maxBufferSize_) {
                logger->log_error("Dropping event %x because it couldn't be rendered within %ll bytes.", hEvent, pConsumeWindowsEventLog->maxBufferSize_);
                return 0UL;
              }

              size = used;
              std::vector<char> buf(size);
              if (EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
                std::string xml = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(reinterpret_cast<wchar_t*>(&buf[0]));

                pConsumeWindowsEventLog->renderedXMLs_.enqueue(std::move(xml));
              } else {
                logger->log_error("EvtRender returned the following error code: %d.", GetLastError());
              }
            }
          }
        }

        return 0UL;
      },
      EvtSubscribeToFutureEvents | EvtSubscribeStrict);

  if (!subscriptionHandle_) {
    logger_->log_error("Unable to subscribe with provided parameters, received the following error code: %d", GetLastError());
    return false;
  }

  lastActivityTimestamp_ = GetTickCount();

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
      : str_(str) {
      status_ = 0;
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      auto len = stream->writeData((uint8_t*)&str_[0], str_.size());
      if (len < 0)
        status_ = -1;
      return len;
    }

    std::string str_;
    int status_;
  };

  int flowFileCount = 0;

  std::string xml;
  while (renderedXMLs_.try_dequeue(xml)) {
    auto flowFile = session->create();

    session->write(flowFile, &WriteCallback(xml));
    session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "application/xml");
    session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session->transfer(flowFile, Success);
    session->commit();

    flowFileCount++;
  }

  return flowFileCount;
}

void ConsumeWindowsEventLog::notifyStop()
{
  unsubscribe();

  if (renderedXMLs_.size_approx() != 0) {
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
