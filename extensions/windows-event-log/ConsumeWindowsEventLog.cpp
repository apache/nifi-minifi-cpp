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
#include <regex>

#include "io/DataStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/OsUtils.h"

#pragma comment(lib, "wevtapi.lib")

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

core::Property ConsumeWindowsEventLog::RenderFormatXML(
  core::PropertyBuilder::createProperty("Render Format XML?")->
  isRequired(true)->
  withDefaultValue<bool>(true)->
  withDescription("Render format XML or Text.)")->
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

core::Relationship ConsumeWindowsEventLog::Success("success", "Relationship for successfully consumed events.");

ConsumeWindowsEventLog::ConsumeWindowsEventLog(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<ConsumeWindowsEventLog>::getLogger()), apply_identifier_function_(false) {
  // Initializes COM for current thread, it is needed to MSXML parser.
  CoInitializeEx(0, COINIT_APARTMENTTHREADED);

  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LogWindowsError();
  }
}

ConsumeWindowsEventLog::~ConsumeWindowsEventLog() {
  if (xmlDoc_) {
    xmlDoc_.Release();
  }
  CoUninitialize();
}

void ConsumeWindowsEventLog::initialize() {
  //! Set the supported properties
  setSupportedProperties({Channel, Query, RenderFormatXML, MaxBufferSize, InactiveDurationToReconnect, IdentifierMatcher, IdentifierFunction });

  //! Set the supported relationships
  setSupportedRelationships({Success});
}

void ConsumeWindowsEventLog::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
	context->getProperty(IdentifierMatcher.getName(), regex_);
	context->getProperty(IdentifierFunction.getName(), apply_identifier_function_);
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

void ConsumeWindowsEventLog::matchRegex(const MSXML2::IXMLDOMElementPtr pRoot, std::map<std::string, std::string> &fieldsAndValues, std::wregex match) {
	const auto pNodeChildren = pRoot->childNodes;
	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;
	for (long i = 0; i < pNodeChildren->length; i++) {
		const auto pNode = pNodeChildren->item[i];
		const auto nodeType = pNode->GetnodeType();
		if (DOMNodeType::NODE_ELEMENT == nodeType) {
			auto nodeName = pNode->nodeName;

			std::wstring wstringNodeName(nodeName, SysStringLen(nodeName));
			bool matched = false;
			if (std::regex_match(wstringNodeName, match)) {
				matched = true;
			}
			else {
				const auto pAttributes = pNode->attributes;
				for (long iAttr = 0; iAttr < pAttributes->length; iAttr++) {
					const auto pAttribute = pAttributes->item[iAttr];
					if (pAttribute->nodeName == static_cast<_bstr_t>("Name")) {
						auto  attributeNodeName = static_cast<_bstr_t>(pAttribute->nodeValue);
						std::wstring wAttributeNodeName(attributeNodeName, SysStringLen(attributeNodeName));
						if (std::regex_match(wAttributeNodeName, match)) {
							wstringNodeName = attributeNodeName;
							matched = true;
						}
					}
				}

			}
			if (matched) {
				auto value = static_cast<_bstr_t>(pNode->Gettext());
				std::wstring wstringValue(value, SysStringLen(value));
				auto matched_field_name = converter.to_bytes(wstringNodeName);
				auto matched_field_value = converter.to_bytes(wstringValue);
				
				fieldsAndValues[matched_field_name] = apply_identifier_function_ ? utils::OsUtils::userIdToUsername(matched_field_value) : matched_field_value;
			}
			matchRegex(pNode, fieldsAndValues, match);
		}
	}
	
}

void ConsumeWindowsEventLog::createTextOutput(const MSXML2::IXMLDOMElementPtr pRoot, std::wstringstream& stream, std::vector<std::wstring>& ancestors) {
  const auto pNodeChildren = pRoot->childNodes;

  auto writeAncestors = [](const std::vector<std::wstring>& ancestors, std::wstringstream& stream) {
    for (size_t j = 0; j < ancestors.size() - 1; j++) {
      stream << ancestors[j] + L'/';
    }
    stream << ancestors.back();
  };

  if (0 == pNodeChildren->length) {
    writeAncestors(ancestors, stream);

    stream << std::endl;
  } else {
    for (long i = 0; i < pNodeChildren->length; i++) {
      std::wstringstream curStream;

      const auto pNode = pNodeChildren->item[i];

      const auto nodeType = pNode->GetnodeType();

      if (DOMNodeType::NODE_TEXT == nodeType) {
        const auto nodeValue = pNode->text;
        if (nodeValue.length()) {
          writeAncestors(ancestors, stream);

          std::wstring strNodeValue = static_cast<LPCWSTR>(nodeValue);

          // Remove '\n', '\r' - just substitute all whitespaces with ' '. 
          strNodeValue = std::regex_replace(strNodeValue, std::wregex(L"\\s+"), L" ");

          curStream << L" = " << strNodeValue;
        }

        stream << curStream.str() << std::endl;
      } else if (DOMNodeType::NODE_ELEMENT == nodeType) {
        curStream << pNode->nodeName;

        const auto pAttributes = pNode->attributes;
        for (long iAttr = 0; iAttr < pAttributes->length; iAttr++) {
          const auto pAttribute = pAttributes->item[iAttr];

          curStream << L" " << pAttribute->nodeName << L'(' << static_cast<_bstr_t>(pAttribute->nodeValue) << L')';
        }

        ancestors.emplace_back(curStream.str());
        createTextOutput(pNode, stream, ancestors);
        ancestors.pop_back();
      }
    }
  }
}

bool ConsumeWindowsEventLog::subscribe(const std::shared_ptr<core::ProcessContext> &context) {
  std::string channel;
  context->getProperty(Channel.getName(), channel);

  std::string query;
  context->getProperty(Query.getName(), query);

  context->getProperty(RenderFormatXML.getName(), renderXML_);

  if (xmlDoc_) {
	  xmlDoc_.Release();
  }

  HRESULT hr = xmlDoc_.CreateInstance(__uuidof(MSXML2::DOMDocument60));
  if (FAILED(hr)) {
      logger_->log_error("!xmlDoc_.CreateInstance %x", hr);
      return false;
  }

  xmlDoc_->async = VARIANT_FALSE;
  xmlDoc_->validateOnParse = VARIANT_FALSE;
  xmlDoc_->resolveExternals = VARIANT_FALSE;
  

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
              std::vector<wchar_t> buf(size/2 + 1);
              if (EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
                const auto xml = to_string(&buf[0]);

				EventRender renderedData;
				if (VARIANT_FALSE == pConsumeWindowsEventLog->xmlDoc_->loadXML(_bstr_t(xml.c_str()))) {
					logger->log_error("'loadXML' failed");
					return 0UL;
				}
				if (!pConsumeWindowsEventLog->regex_.empty()) {
					std::wstring wstring_regex_;
					wstring_regex_.assign(pConsumeWindowsEventLog->regex_.begin(), pConsumeWindowsEventLog->regex_.end());
					std::wregex  regex(wstring_regex_);
					pConsumeWindowsEventLog->matchRegex(pConsumeWindowsEventLog->xmlDoc_->documentElement, renderedData.matched_fields_, regex);
				}

                if (pConsumeWindowsEventLog->renderXML_) {
					renderedData.text_ = std::move(xml);
                } else {

                  std::wstringstream stream;
                  std::vector<std::wstring> ancestors;
                  pConsumeWindowsEventLog->createTextOutput(pConsumeWindowsEventLog->xmlDoc_->documentElement, stream, ancestors);
				  renderedData.text_ = std::move(to_string(stream.str().c_str()));
                }
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
