/**
 * @file CollectorInitiatedSubscription.cpp
 CollectorInitiatedSubscription class implementation
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

#include "CollectorInitiatedSubscription.h"
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <memory>
#include <codecvt>
#include <utility>

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

#include "utils/gsl.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Wecapi.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define LOG_SUBSCRIPTION_ERROR(error) logError(__LINE__, error)
#define LOG_SUBSCRIPTION_WINDOWS_ERROR(info) logWindowsError(__LINE__, info)

static std::string to_string(const wchar_t* pChar) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pChar);
}

const std::string ProcessorName("CollectorInitiatedSubscription");

//! Supported Relationships
static core::Relationship s_success("success", "Relationship for successfully consumed events.");


CollectorInitiatedSubscription::CollectorInitiatedSubscription(const std::string& name, const utils::Identifier& uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<CollectorInitiatedSubscription>::getLogger()) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("GetComputerName");
  }

  supportedProperties_ = {
    subscriptionName_ = {
      core::PropertyBuilder::createProperty("Subscription Name")->
      isRequired(true)->
      withDescription("The name of the subscription. The value provided for this parameter should be unique within the computer's scope.")->
      supportsExpressionLanguage(true)->
      build()
    },

    subscriptionDescription_ = {
      core::PropertyBuilder::createProperty("Subscription Description")->
      isRequired(true)->
      withDescription("A description of the subscription.")->
      supportsExpressionLanguage(true)->
      build()
    },

    sourceAddress_ = {
      core::PropertyBuilder::createProperty("Source Address")->
      isRequired(true)->
      withDescription("The IP address or fully qualified domain name (FQDN) of the local or remote computer (event source) from which the events are collected.")->
      supportsExpressionLanguage(true)->
      build()
    },

    sourceUserName_ = {
      core::PropertyBuilder::createProperty("Source User Name")->
      isRequired(true)->
      withDescription("The user name, which is used by the remote computer (event source) to authenticate the user.")->
      supportsExpressionLanguage(true)->
      build()
    },

    sourcePassword_ = {
      core::PropertyBuilder::createProperty("Source Password")->
      isRequired(true)->
      withDescription("The password, which is used by the remote computer (event source) to authenticate the user.")->
      supportsExpressionLanguage(true)->
      build()
    },

    sourceChannels_ = {
      core::PropertyBuilder::createProperty("Source Channels")->
      isRequired(true)->
      withDescription("The Windows Event Log Channels (on domain computer(s)) from which events are transferred.")->
      supportsExpressionLanguage(true)->
      build()
    },

    maxDeliveryItems_ = {
      core::PropertyBuilder::createProperty("Max Delivery Items")->
      isRequired(true)->
      withDefaultValue<core::DataSizeValue>("1000")->
      withDescription("Determines the maximum number of items that will forwarded from an event source for each request.")->
      build()
    },

    deliveryMaxLatencyTime_ = {
      core::PropertyBuilder::createProperty("Delivery MaxLatency Time")->
      isRequired(true)->
      withDefaultValue<core::TimePeriodValue>("10 min")->
      withDescription("How long, in milliseconds, the event source should wait before sending events.")->
      build()
    },

    heartbeatInterval_ = {
      core::PropertyBuilder::createProperty("Heartbeat Interval")->
      isRequired(true)->
      withDefaultValue<core::TimePeriodValue>("10 min")->
      withDescription(
        "Time interval, in milliseconds, which is observed between the sent heartbeat messages."
        " The event collector uses this property to determine the interval between queries to the event source.")->
      build()
    },

    channel_ = {
      core::PropertyBuilder::createProperty("Channel")->
      isRequired(true)->
      withDefaultValue("ForwardedEvents")->
      withDescription("The Windows Event Log Channel (on local machine) to which events are transferred.")->
      supportsExpressionLanguage(true)->
      build()
    },

    query_ = {
      core::PropertyBuilder::createProperty("Query")->
      isRequired(true)->
      withDefaultValue("*")->
      withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")->
      supportsExpressionLanguage(true)->
      build()
    },

    maxBufferSize_ = {
      core::PropertyBuilder::createProperty("Max Buffer Size")->
      isRequired(true)->
      withDefaultValue<core::DataSizeValue>("1 MB")->
      withDescription(
        "The individual Event Log XMLs are rendered to a buffer."
        " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")->
      build()
    },

    inactiveDurationToReconnect_ = {
      core::PropertyBuilder::createProperty("Inactive Duration To Reconnect")->
      isRequired(true)->
      withDefaultValue<core::TimePeriodValue>("10 min")->
      withDescription(
        "If no new event logs are processed for the specified time period,"
        " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
        " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
        " Setting no duration, e.g. '0 ms' disables auto-reconnection.")->
      build()
    }
  };
}

void CollectorInitiatedSubscription::initialize() {
  //! Set the supported properties
  setSupportedProperties(supportedProperties_.getProperties());

  //! Set the supported relationships
  setSupportedRelationships({s_success});
}

void CollectorInitiatedSubscription::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (subscriptionHandle_) {
    logger_->log_error("Processor already subscribed to Event Log, expected cleanup to unsubscribe.");
  } else {
    sessionFactory_ = sessionFactory;

    if (!createSubscription(context))
      return;
    if (!checkSubscriptionRuntimeStatus())
      return;

    subscribe(context);
  }
}

void CollectorInitiatedSubscription::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (!subscriptionHandle_) {
    if (!subscribe(context)) {
      context->yield();
      return;
    }
  }

  // Check subscription runtime status.
  checkSubscriptionRuntimeStatus();

  const auto flowFileCount = processQueue(session);

  const auto now = GetTickCount64();

  if (flowFileCount > 0) {
    lastActivityTimestamp_ = now;
  } else if (inactiveDurationToReconnect_.value() > 0) {
    if ((now - lastActivityTimestamp_) > inactiveDurationToReconnect_.value()) {
      logger_->log_info("Exceeds configured 'inactive duration to reconnect' %lld ms. Unsubscribe to reconnect..", inactiveDurationToReconnect_.value());
      unsubscribe();
    }
  }
}

void CollectorInitiatedSubscription::logInvalidSubscriptionPropertyType(int line, DWORD type) {
  logError(line, "Invalid property type: " + std::to_string(type));
}

bool CollectorInitiatedSubscription::checkSubscriptionRuntimeStatus() {
  EC_HANDLE hSubscription = EcOpenSubscription(subscriptionName_.value().c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
  if (!hSubscription) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcOpenSubscription");
    return false;
  }
  const auto guard_hSubscription = gsl::finally([hSubscription]() { EcClose(hSubscription); });

  PEC_VARIANT vProperty = NULL;
  std::vector<BYTE> buffer;
  if (!getSubscriptionProperty(hSubscription, EcSubscriptionEventSources, 0, buffer, vProperty)) {
    return false;
  }

  // Ensure that we have obtained handle to the Array Property.
  if (vProperty->Type != EcVarTypeNull && vProperty->Type != EcVarObjectArrayPropertyHandle) {
    logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
    return false;
  }

  if (vProperty->Type == EcVarTypeNull) {
    LOG_SUBSCRIPTION_ERROR("!hArray");
    return false;
  }

  const EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray = vProperty->PropertyHandleVal;
  const auto guard_hArray = gsl::finally([hArray]() { EcClose(hArray); });

  // Get the EventSources array size (number of elements).
  DWORD dwEventSourceCount{};
  if (!EcGetObjectArraySize(hArray, &dwEventSourceCount)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArraySize");
    return false;
  }

  auto getArrayProperty = [this](EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray, EC_SUBSCRIPTION_PROPERTY_ID propID, DWORD arrayIndex, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vProperty) -> bool {
    buffer.clear();
    buffer.resize(sizeof(EC_VARIANT));
    DWORD dwBufferSizeUsed{};
    if (!EcGetObjectArrayProperty(hArray, propID, arrayIndex, flags, static_cast<DWORD>(buffer.size()), reinterpret_cast<PEC_VARIANT>(&buffer[0]), &dwBufferSizeUsed)) {
      if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
        buffer.resize(dwBufferSizeUsed);
        if (!EcGetObjectArrayProperty(hArray, propID, arrayIndex, flags, static_cast<DWORD>(buffer.size()), reinterpret_cast<PEC_VARIANT>(&buffer[0]), &dwBufferSizeUsed)) {
          LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArrayProperty");
          return false;
        }
      } else {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArrayProperty");
        return false;
      }
    }

    vProperty = reinterpret_cast<PEC_VARIANT>(&buffer[0]);

    return true;
  };

  auto getStatus = [this](const std::wstring& eventSource, EC_SUBSCRIPTION_RUNTIME_STATUS_INFO_ID statusInfoID, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vStatus) -> bool {
    buffer.clear();
    buffer.resize(sizeof(EC_VARIANT));
    DWORD dwBufferSize{};
    if (!EcGetSubscriptionRunTimeStatus(
          subscriptionName_.value().c_str(),
          statusInfoID,
          eventSource.c_str(),
          flags,
          static_cast<DWORD>(buffer.size()),
          reinterpret_cast<PEC_VARIANT>(&buffer[0]),
          &dwBufferSize)) {
      if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
        buffer.resize(dwBufferSize);
        if (!EcGetSubscriptionRunTimeStatus(subscriptionName_.value().c_str(),
              statusInfoID,
              eventSource.c_str(),
              flags,
              static_cast<DWORD>(buffer.size()),
              reinterpret_cast<PEC_VARIANT>(&buffer[0]),
              &dwBufferSize)) {
          LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionRunTimeStatus");
        }
      } else {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionRunTimeStatus");
      }
    }

    vStatus = reinterpret_cast<PEC_VARIANT>(&buffer[0]);

    return true;
  };

  for (DWORD i = 0; i < dwEventSourceCount; i++) {
    std::vector<BYTE> eventSourceBuffer;
    PEC_VARIANT vProperty = NULL;
    if (!getArrayProperty(hArray, EcSubscriptionEventSourceAddress, i, 0, eventSourceBuffer, vProperty)) {
      return false;
    }

    if (vProperty->Type != EcVarTypeNull && vProperty->Type != EcVarTypeString) {
      logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
      return false;
    }

    if (vProperty->Type == EcVarTypeNull)
      continue;

    const std::wstring eventSource = vProperty->StringVal;

    if (!getStatus(eventSource.c_str(), EcSubscriptionRunTimeStatusActive, 0, buffer, vProperty)) {
      return false;
    }

    if (vProperty->Type != EcVarTypeUInt32) {
      logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
      return false;
    }

    const auto runtimeStatus = vProperty->UInt32Val;

    std::wstring strRuntimeStatus;

    switch (runtimeStatus) {
    case EcRuntimeStatusActiveStatusActive:
      strRuntimeStatus = L"Active";
      break;
    case EcRuntimeStatusActiveStatusDisabled:
      strRuntimeStatus = L"Disabled";
      break;
    case EcRuntimeStatusActiveStatusInactive:
      strRuntimeStatus = L"Inactive";
      break;
    case EcRuntimeStatusActiveStatusTrying:
      strRuntimeStatus = L"Trying";
      break;
    default:
      strRuntimeStatus = L"Unknown";
      break;
    }

    // Get Subscription Last Error.
    if (!getStatus(eventSource, EcSubscriptionRunTimeStatusLastError, 0, buffer, vProperty)) {
      return false;
    }

    if (vProperty->Type != EcVarTypeUInt32) {
      logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
      return false;
    }

    const auto lastError = vProperty->UInt32Val;

    if (lastError == 0 && (runtimeStatus == EcRuntimeStatusActiveStatusActive || runtimeStatus == EcRuntimeStatusActiveStatusTrying)) {
      logger_->log_info("Subscription '%ws': status '%ws', no error.", subscriptionName_.value().c_str(), strRuntimeStatus.c_str());
      return true;
    }

    // Obtain the associated Error Message.
    if (!getStatus(eventSource, EcSubscriptionRunTimeStatusLastErrorMessage, 0, buffer, vProperty)) {
      return false;
    }

    if (vProperty->Type != EcVarTypeNull && vProperty->Type != EcVarTypeString) {
      logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
      return false;
    }

    std::wstring lastErrorMessage;
    if (vProperty->Type == EcVarTypeString) {
      lastErrorMessage = vProperty->StringVal;
    }

    logger_->log_error("Runtime status: %ws, last error: %d, last error message: %ws", strRuntimeStatus.c_str(), lastError, lastErrorMessage.c_str());

    return false;
  }

  return true;
}

bool CollectorInitiatedSubscription::getSubscriptionProperty(EC_HANDLE hSubscription, EC_SUBSCRIPTION_PROPERTY_ID propID, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vProperty) {
  buffer.clear();
  buffer.resize(sizeof(EC_VARIANT));
  DWORD dwBufferSize{};
  if (!EcGetSubscriptionProperty(hSubscription, propID, flags, static_cast<DWORD>(buffer.size()), reinterpret_cast<PEC_VARIANT>(&buffer[0]), &dwBufferSize)) {
    if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
      buffer.resize(dwBufferSize);
      if (!EcGetSubscriptionProperty(hSubscription, propID, flags, static_cast<DWORD>(buffer.size()), reinterpret_cast<PEC_VARIANT>(&buffer[0]), &dwBufferSize)) {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionProperty");
        return false;
      }
    } else {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionProperty");
      return false;
    }
  }

  vProperty = reinterpret_cast<PEC_VARIANT>(&buffer[0]);

  return true;
}

bool CollectorInitiatedSubscription::createSubscription(const std::shared_ptr<core::ProcessContext>& context) {
  supportedProperties_.init(context);

  // If subcription already exists, delete it.
  EC_HANDLE hSubscription = EcOpenSubscription(subscriptionName_.value().c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
  if (hSubscription) {
    EcClose(hSubscription);
    if (!EcDeleteSubscription(subscriptionName_.value().c_str(), 0)) {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcDeleteSubscription");
      return false;
    }
  }

  // Create subscription.
  hSubscription = EcOpenSubscription(subscriptionName_.value().c_str(), EC_READ_ACCESS | EC_WRITE_ACCESS, EC_CREATE_NEW);
  if (!hSubscription) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcOpenSubscription");
    return false;
  }
  const auto guard_hSubscription = gsl::finally([hSubscription]() { EcClose(hSubscription); });

  struct SubscriptionProperty {
    SubscriptionProperty(EC_SUBSCRIPTION_PROPERTY_ID propId, const std::wstring& val) {
      propId_ = propId;

      prop_.Type = EcVarTypeString;
      prop_.StringVal = val.c_str();
    }

    SubscriptionProperty(EC_SUBSCRIPTION_PROPERTY_ID propId, uint32_t val) {
      propId_ = propId;

      prop_.Type = EcVarTypeUInt32;
      prop_.UInt32Val = val;
    }

    SubscriptionProperty(EC_SUBSCRIPTION_PROPERTY_ID propId, bool val) {
      propId_ = propId;

      prop_.Type = EcVarTypeBoolean;
      prop_.BooleanVal = val;
    }

    EC_SUBSCRIPTION_PROPERTY_ID propId_;
    EC_VARIANT prop_;
  };

  std::vector<SubscriptionProperty> listProperty = {
    {EcSubscriptionDescription, subscriptionDescription_.value()},
    {EcSubscriptionURI, std::wstring(L"http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog")},
    {EcSubscriptionQuery, L"<QueryList><Query Path=\"" + sourceChannels_.value() + L"\"><Select>*</Select></Query></QueryList>"},
    {EcSubscriptionLogFile, channel_.value()},
    {EcSubscriptionConfigurationMode, static_cast<uint32_t>(EcConfigurationModeCustom)},
    {EcSubscriptionDeliveryMode, static_cast<uint32_t>(EcDeliveryModePull)},
    {EcSubscriptionDeliveryMaxItems, static_cast<uint32_t>(maxDeliveryItems_.value())},
    {EcSubscriptionDeliveryMaxLatencyTime, static_cast<uint32_t>(deliveryMaxLatencyTime_.value())},
    {EcSubscriptionHeartbeatInterval, static_cast<uint32_t>(heartbeatInterval_.value())},
    {EcSubscriptionContentFormat, static_cast<uint32_t>(EcContentFormatRenderedText)},
    {EcSubscriptionCredentialsType, static_cast<uint32_t>(EcSubscriptionCredDefault)},
    {EcSubscriptionEnabled, true},
    {EcSubscriptionCommonUserName, sourceUserName_.value()},
    {EcSubscriptionCommonPassword, sourcePassword_.value()}
  };
  for (auto& prop : listProperty) {
    if (!EcSetSubscriptionProperty(hSubscription, prop.propId_, 0, &prop.prop_)) {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcSetSubscriptionProperty id: " + std::to_string(prop.propId_));
      return false;
    }
  }

  // Get the EventSources array so a new event source can be added for the specified target.
  std::vector<BYTE> buffer;
  PEC_VARIANT vProperty = NULL;
  if (!getSubscriptionProperty(hSubscription, EcSubscriptionEventSources, 0, buffer, vProperty))
    return false;

  // Event Sources is a collection. Ensure that we have obtained handle to the Array Property.
  if (vProperty->Type != EcVarTypeNull && vProperty->Type != EcVarObjectArrayPropertyHandle) {
    logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
    return false;
  }

  if (vProperty->Type == EcVarTypeNull) {
    LOG_SUBSCRIPTION_ERROR("!hArray");
    return false;
  }

  const EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray = vProperty->PropertyHandleVal;
  const auto guard_hArray = gsl::finally([hArray]() { EcClose(hArray); });

  DWORD dwEventSourceCount{};
  if (!EcGetObjectArraySize(hArray, &dwEventSourceCount)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArraySize");
    return false;
  }

  // Add a new EventSource to the EventSources array object.
  if (!EcInsertObjectArrayElement(hArray, dwEventSourceCount)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcInsertObjectArrayElement");
    return false;
  }

  for (auto& prop : std::vector<SubscriptionProperty>{{EcSubscriptionEventSourceAddress, sourceAddress_.value()}, {EcSubscriptionEventSourceEnabled, true}}) {
    if (!EcSetObjectArrayProperty(hArray, prop.propId_, dwEventSourceCount, 0, &prop.prop_)) {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcSetObjectArrayProperty id: " + std::to_string(prop.propId_));
      return false;
    }
  }

  if (!EcSaveSubscription(hSubscription, NULL)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcSaveSubscription");
    return false;
  }

  return true;
}

bool CollectorInitiatedSubscription::subscribe(const std::shared_ptr<core::ProcessContext> &context) {
  logger_->log_debug("CollectorInitiatedSubscription: maxBufferSize_ %lld", maxBufferSize_.value());

  provenanceUri_ = "winlog://" + computerName_ + "/" + to_string(channel_.value().c_str()) + "?" + to_string(query_.value().c_str());

  subscriptionHandle_ = EvtSubscribe(
      NULL,
      NULL,
      channel_.value().c_str(),
      query_.value().c_str(),
      NULL,
      this,
      [](EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) {
        auto pCollectorInitiatedSubscription = static_cast<CollectorInitiatedSubscription*>(pContext);

        auto& logger = pCollectorInitiatedSubscription->logger_;

        if (action == EvtSubscribeActionError) {
          if (ERROR_EVT_QUERY_RESULT_STALE == reinterpret_cast<intptr_t>(hEvent)) {
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
              if (used > pCollectorInitiatedSubscription->maxBufferSize_.value()) {
                logger->log_error("Dropping event %p because it couldn't be rendered within %llu bytes.", hEvent, pCollectorInitiatedSubscription->maxBufferSize_.value());
                return 0UL;
              }

              size = used;
              std::vector<wchar_t> buf(size/2);
              if (EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
                auto xml = to_string(&buf[0]);

                pCollectorInitiatedSubscription->renderedXMLs_.enqueue(std::move(xml));
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

void CollectorInitiatedSubscription::unsubscribe() {
  if (subscriptionHandle_) {
    EvtClose(subscriptionHandle_);
    subscriptionHandle_ = 0;
  }
}

int CollectorInitiatedSubscription::processQueue(const std::shared_ptr<core::ProcessSession> &session) {
  struct WriteCallback: public OutputStreamCallback {
    explicit WriteCallback(const std::string& str)
      : str_(&str) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
      const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(str_->data()), str_->size());
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }

    gsl::not_null<const std::string*> str_;
  };

  int flowFileCount = 0;

  std::string xml;
  while (renderedXMLs_.try_dequeue(xml)) {
    auto flowFile = session->create();

    {
      WriteCallback wc{ xml };
      session->write(flowFile, &wc);
    }
    session->putAttribute(flowFile, core::SpecialFlowAttribute::MIME_TYPE, "application/xml");
    session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0);
    session->transfer(flowFile, s_success);
    session->commit();

    flowFileCount++;
  }

  return flowFileCount;
}

void CollectorInitiatedSubscription::notifyStop() {
  if (!EcDeleteSubscription(subscriptionName_.value().c_str(), 0)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcDeleteSubscription");
  }

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

void CollectorInitiatedSubscription::logError(int line, const std::string& error) {
  logger_->log_error("Line %d: %s\n", error.c_str());
}

void CollectorInitiatedSubscription::logWindowsError(int line, const std::string& info) {
  auto error = GetLastError();

  LPVOID lpMsg{};
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
    NULL,
    error,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&lpMsg,
    0,
    NULL);

  logger_->log_error("Line %d: '%s': error %d: %s\n", line, info.c_str(), static_cast<int>(error), reinterpret_cast<char *>(lpMsg));

  LocalFree(lpMsg);
}

REGISTER_RESOURCE(CollectorInitiatedSubscription, "Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
