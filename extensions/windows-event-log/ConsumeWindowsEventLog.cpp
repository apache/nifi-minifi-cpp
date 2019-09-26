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

#include "io/DataStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"


#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {



static std::string to_string(const wchar_t* pChar) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pChar);
}

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

core::Relationship ConsumeWindowsEventLog::Success("success", "Relationship for successfully consumed events.");

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger()), apply_identifier_function_(false) {

  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LogWindowsError();
  }
}

ConsumeWindowsEventLog::~ConsumeWindowsEventLog() {
}

void ConsumeWindowsEventLog::initialize() {
  //! Set the supported properties
  setSupportedProperties({Channel, Query, MaxBufferSize, InactiveDurationToReconnect, IdentifierMatcher, IdentifierFunction, ResolveAsAttributes, EventHeaderDelimiter, EventHeader });

  //! Set the supported relationships
  setSupportedRelationships({Success});
}

bool ConsumeWindowsEventLog::insertHeaderName(wel::METADATA_NAMES &header, const std::string &key, const std::string & value) {
	
	wel::METADATA name = wel::WindowsEventLogMetadata::getMetadataFromString(key);
		
	if (name != wel::METADATA::UNKNOWN) {
		header.emplace_back(std::make_pair(name, value));
		return true;
	}
	else {
		return false;
	}
}

void ConsumeWindowsEventLog::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
	context->getProperty(IdentifierMatcher.getName(), regex_);
	context->getProperty(ResolveAsAttributes.getName(), resolve_as_attributes_);
	context->getProperty(IdentifierFunction.getName(), apply_identifier_function_);
	context->getProperty(EventHeaderDelimiter.getName(), header_delimiter_);

	std::string header;
	context->getProperty(EventHeader.getName(), header);
	
	auto keyValueSplit = utils::StringUtils::split(header, ",");
	for (const auto &kv : keyValueSplit) {
		auto splitKeyAndValue = utils::StringUtils::split(kv, "=");
		if (splitKeyAndValue.size() == 2) {
			auto key = utils::StringUtils::trim(splitKeyAndValue.at(0));
			auto value = utils::StringUtils::trim(splitKeyAndValue.at(1));
			if (!insertHeaderName(header_names_, key, value)) {
				logger_->log_debug("%s is an invalid key for the header map", key);
			}
		}
		else if (splitKeyAndValue.size() == 1) {
			auto key = utils::StringUtils::trim(splitKeyAndValue.at(0));
			if (!insertHeaderName(header_names_, key, "")) {
				logger_->log_debug("%s is an invalid key for the header map", key);
			}
		}

	}
	
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


bool ConsumeWindowsEventLog::subscribe(const std::shared_ptr<core::ProcessContext> &context) {
  context->getProperty(Channel.getName(), channel_);

  std::string query;
  context->getProperty(Query.getName(), query);

  context->getProperty(MaxBufferSize.getName(), maxBufferSize_);
  logger_->log_debug("ConsumeWindowsEventLog: maxBufferSize_ %lld", maxBufferSize_);

  provenanceUri_ = "winlog://" + computerName_ + "/" + channel_ + "?" + query;

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
      std::wstring(channel_.begin(), channel_.end()).c_str(),
      std::wstring(query.begin(), query.end()).c_str(),
      NULL,
      this,
      [](EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE eventHandle)
      {
	  
        auto pConsumeWindowsEventLog = static_cast<ConsumeWindowsEventLog*>(pContext);

        auto& logger = pConsumeWindowsEventLog->logger_;

        if (action == EvtSubscribeActionError) {
          if (ERROR_EVT_QUERY_RESULT_STALE == (DWORD)eventHandle) {
            logger->log_error("Received missing event notification. Consider triggering processor more frequently or increasing queue size.");
          } else {
            logger->log_error("Received the following Win32 error: %x", eventHandle);
          }
        } else if (action == EvtSubscribeActionDeliver) {
          DWORD size = 0;
          DWORD used = 0;
          DWORD propertyCount = 0;
          if (!EvtRender(NULL, eventHandle, EvtRenderEventXml, size, 0, &used, &propertyCount)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
              if (used > pConsumeWindowsEventLog->maxBufferSize_) {
                logger->log_error("Dropping event %x because it couldn't be rendered within %ll bytes.", eventHandle, pConsumeWindowsEventLog->maxBufferSize_);
                return 0UL;
              }

              size = used;
              std::vector<wchar_t> buf(size/2 + 1);
              if (EvtRender(NULL, eventHandle, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
                std::string xml = to_string(&buf[0]);

				EventRender renderedData;

				pugi::xml_document doc;
				pugi::xml_parse_result result = doc.load_string(xml.c_str());

				if (!result) {
					logger->log_error("Invalid XML produced");
					return 0UL;
				}
				// this is a well known path. 
				std::string providerName = doc.child("Event").child("System").child("Provider").attribute("Name").value();
				
				auto handler = pConsumeWindowsEventLog->getEventLogHandler(providerName);
				auto message = handler.getEventMessage(eventHandle);

				

				// resolve the event metadata
				wel::MetadataWalker walker(pConsumeWindowsEventLog->getEventLogHandler(providerName).getMetadata(), pConsumeWindowsEventLog->channel_, eventHandle, !pConsumeWindowsEventLog->resolve_as_attributes_, pConsumeWindowsEventLog->apply_identifier_function_, pConsumeWindowsEventLog->regex_);
				doc.traverse(walker);

				if (!message.empty())
				{	
					for (const auto &mapEntry : walker.getIdentifiers()) {
						// replace the identifiers with their translated strings.
						utils::StringUtils::replaceAll(message, mapEntry.first, mapEntry.second);
					}
					wel::WindowsEventLogHeader log_header(pConsumeWindowsEventLog->header_names_);
					// set the delimiter
					log_header.setDelimiter(pConsumeWindowsEventLog->header_delimiter_);
					// render the header.
					renderedData.rendered_text_ = log_header.getEventHeader(&walker);
					renderedData.rendered_text_ += "Message" + pConsumeWindowsEventLog->header_delimiter_ + " ";
					renderedData.rendered_text_ += message;
				}

				if (pConsumeWindowsEventLog->resolve_as_attributes_) {
					renderedData.matched_fields_ = walker.getFieldValues();
				}

				wel::XmlString writer;
				doc.print(writer,"", pugi::format_raw); // no indentation or formatting
				xml = writer.xml_;
				
				renderedData.text_ = std::move(xml);

				pConsumeWindowsEventLog->listRenderedData_.enqueue(std::move(renderedData));
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
      : str_(str) {
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      return stream->writeData((uint8_t*)&str_[0], str_.size());
    }

    std::string str_;
  };

  int flowFileCount = 0;
 
  EventRender evt;
  while (listRenderedData_.try_dequeue(evt)) {
    auto flowFile = session->create();

    session->write(flowFile, &WriteCallback(evt.text_));
	for (const auto &fieldMapping : evt.matched_fields_) {
		if (!fieldMapping.second.empty()) {
			session->putAttribute(flowFile, fieldMapping.first, fieldMapping.second);
		}
	}
    session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "application/xml");
    session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session->transfer(flowFile, Success);

	flowFile = session->create();

	session->write(flowFile, &WriteCallback(evt.rendered_text_));
	session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
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
