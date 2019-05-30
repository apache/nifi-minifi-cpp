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
#include <vector>
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
#include "utils/ScopeGuard.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Wecapi.lib")

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define LOG_SUBSCRIPTION_ERROR(error) logError(__LINE__, error)
#define LOG_SUBSCRIPTION_WINDOWS_ERROR(info) logWindowsError(__LINE__, info)

struct ISupportedProperty {
  void virtual Init(const std::shared_ptr<core::ProcessContext>& context) = 0;
};

class SuportedProperties {
  std::vector<ISupportedProperty*> vISupportedProperty_;
  std::set<core::Property> sSupportedProperties_;

public:
  template <typename T>
  void add(T& t)
  {
    vISupportedProperty_.emplace_back(&t);
    sSupportedProperties_.insert(t);
  }

  void init(const std::shared_ptr<core::ProcessContext>& context) {
    for (auto pProp : vISupportedProperty_) {
      pProp->Init(context);
    }
  }

  operator std::set<core::Property>() const
  {
    return sSupportedProperties_;
  }
};

static SuportedProperties s_supportedProperties;

template <typename T>
class SupportedProperty: public ISupportedProperty, public core::Property {
  T t_;

public:
  template <typename ...Args>
  SupportedProperty(const Args& ...args) : core::Property(args...) {
    s_supportedProperties.add(*this);
  }

  void Init(const std::shared_ptr<core::ProcessContext>& context) override;

  T value() {
    return t_;
  }
};

void SupportedProperty<std::wstring>::Init(const std::shared_ptr<core::ProcessContext>& context) {
  std::string val;
  context->getProperty(getName(), val);

  t_ = std::wstring(val.begin(), val.end());
};

void SupportedProperty<uint64_t>::Init(const std::shared_ptr<core::ProcessContext>& context) {
  context->getProperty(getName(), t_);
};

struct TimeProperty {
  int64_t time_{};

  operator int64_t() {
    return time_;
  }
};

void SupportedProperty<TimeProperty>::Init(const std::shared_ptr<core::ProcessContext>& context) {
  std::string strTime;
  context->getProperty(getName(), strTime);

  int64_t time{};
  core::TimeUnit unit;
  core::Property::StringToTime(strTime, time, unit);
  core::Property::ConvertTimeUnitToMS(time, unit, time);

  t_.time_ = time;
};

static auto to_string(const std::wstring& str) {
  return std::string(str.begin(), str.end());
}

static auto to_wstring(const std::string& str) {
  return std::wstring(str.begin(), str.end());
}

const std::string ProcessorName("CollectorInitiatedSubscription");

//! Supported Properties
static SupportedProperty<std::wstring> s_subscriptionName(
  core::PropertyBuilder::createProperty("Subscription Name")->
  isRequired(true)->
  withDescription("The name of the subscription. The value provided for this parameter should be unique within the computer's scope.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_subscriptionDescription(
  core::PropertyBuilder::createProperty("Subscription Description")->
  isRequired(true)->
  withDescription("A description of the subscription.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_sourceAddress(
  core::PropertyBuilder::createProperty("Source Address")->
  isRequired(true)->
  withDescription("The IP address or fully qualified domain name (FQDN) of the local or remote computer (event source) from which the events are collected.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_sourceUserName(
  core::PropertyBuilder::createProperty("Source User Name")->
  isRequired(true)->
  withDescription("The user name, which is used by the remote computer (event source) to authenticate the user.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_sourcePassword(
  core::PropertyBuilder::createProperty("Source Password")->
  isRequired(true)->
  withDescription("The password, which is used by the remote computer (event source) to authenticate the user.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_sourceChannels(
  core::PropertyBuilder::createProperty("Source Channels")->
  isRequired(true)->
  withDescription("The Windows Event Log Channels (on domain computer(s)) from which events are transferred.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<uint64_t> s_maxDeliveryItems(
  core::PropertyBuilder::createProperty("Max Delivery Items")->
  isRequired(true)->
  withDefaultValue<core::DataSizeValue>("1000")->
  withDescription("Determines the maximum number of items that will forwarded from an event source for each request.")->
  build());

static SupportedProperty<TimeProperty> s_deliveryMaxLatencyTime(
  core::PropertyBuilder::createProperty("Delivery MaxLatency Time")->
  isRequired(true)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  withDescription("How long, in milliseconds, the event source should wait before sending events.")->
  build());

static SupportedProperty<TimeProperty> s_heartbeatInterval(
  core::PropertyBuilder::createProperty("Heartbeat Interval")->
  isRequired(true)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  withDescription(
    "Time interval, in milliseconds, which is observed between the sent heartbeat messages."
    " The event collector uses this property to determine the interval between queries to the event source.")->
  build());

static SupportedProperty<std::wstring> s_channel(
  core::PropertyBuilder::createProperty("Channel")->
  isRequired(true)->
  withDefaultValue("ForwardedEvents")->
  withDescription("The Windows Event Log Channel (on local machine) to which events are transferred.")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<std::wstring> s_query(
  core::PropertyBuilder::createProperty("Query")->
  isRequired(true)->
  withDefaultValue("*")->
  withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")->
  supportsExpressionLanguage(true)->
  build());

static SupportedProperty<uint64_t> s_maxBufferSize(
  core::PropertyBuilder::createProperty("Max Buffer Size")->
  isRequired(true)->
  withDefaultValue<core::DataSizeValue>("1 MB")->
  withDescription(
    "The individual Event Log XMLs are rendered to a buffer."
    " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")->
  build());

static SupportedProperty<TimeProperty> s_inactiveDurationToReconnect(
  core::PropertyBuilder::createProperty("Inactive Duration To Reconnect")->
  isRequired(true)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  withDescription(
    "If no new event logs are processed for the specified time period,"
    " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
    " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
    " Setting no duration, e.g. '0 ms' disables auto-reconnection.")->
  build());

//! Supported Relationships
static core::Relationship s_success("success", "Relationship for successfully consumed events.");


CollectorInitiatedSubscription::CollectorInitiatedSubscription(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<CollectorInitiatedSubscription>::getLogger()) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("GetComputerName");
  }
}

void CollectorInitiatedSubscription::initialize() {
  //! Set the supported properties
  setSupportedProperties(s_supportedProperties);

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
  }
  else if (s_inactiveDurationToReconnect.value() > 0) {
    if ((now - lastActivityTimestamp_) > s_inactiveDurationToReconnect.value()) {
      logger_->log_info("Exceeds configured 'inactive duration to reconnect' %lld ms. Unsubscribe to reconnect..", s_inactiveDurationToReconnect.value());
      unsubscribe();
    }
  }
}

void CollectorInitiatedSubscription::logInvalidSubscriptionPropertyType(int line, DWORD type) {
  logError(line, "Invalid property type: " + std::to_string(type));
}

bool CollectorInitiatedSubscription::checkSubscriptionRuntimeStatus() {
  EC_HANDLE hSubscription = EcOpenSubscription(s_subscriptionName.value().c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
  if (!hSubscription) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcOpenSubscription");
    return false;
  }
  utils::ScopeGuard guard_hSubscription([hSubscription]() { EcClose(hSubscription); });

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

  EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray = vProperty->PropertyHandleVal;
  utils::ScopeGuard guard_hArray([hArray]() { EcClose(hArray); });

  // Get the EventSources array size (number of elements).
  DWORD dwEventSourceCount{};
  if (!EcGetObjectArraySize(hArray, &dwEventSourceCount)) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArraySize");
    return false;
  }

  auto getArrayProperty = [this](EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray, EC_SUBSCRIPTION_PROPERTY_ID propID, DWORD arrayIndex, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vProperty)
  {
    buffer.clear();
    buffer.resize(sizeof(EC_VARIANT));
    DWORD dwBufferSizeUsed{};
    if (!EcGetObjectArrayProperty(hArray, propID, arrayIndex, flags, (DWORD)buffer.size(), (PEC_VARIANT)&buffer[0], &dwBufferSizeUsed)) {
      if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
        buffer.resize(dwBufferSizeUsed);
        if (!EcGetObjectArrayProperty(hArray, propID, arrayIndex, flags, (DWORD)buffer.size(), (PEC_VARIANT)&buffer[0], &dwBufferSizeUsed)) {
          LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArrayProperty");
          return false;
        }
      }
      else {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetObjectArrayProperty");
        return false;
      }
    }

    vProperty = (PEC_VARIANT)&buffer[0];

    return true;
  };

  auto getStatus = [this](const std::wstring& eventSource, EC_SUBSCRIPTION_RUNTIME_STATUS_INFO_ID statusInfoID, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vStatus) {
    buffer.clear();
    buffer.resize(sizeof(EC_VARIANT));
    DWORD dwBufferSize{};
    if (!EcGetSubscriptionRunTimeStatus(
          s_subscriptionName.value().c_str(), 
          statusInfoID, 
          eventSource.c_str(),
          flags,
          (DWORD)buffer.size(),
          (PEC_VARIANT)&buffer[0],
          &dwBufferSize)) {
      if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
        buffer.resize(dwBufferSize);
        if (!EcGetSubscriptionRunTimeStatus(s_subscriptionName.value().c_str(),
              statusInfoID,
              eventSource.c_str(),
              flags,
              (DWORD)buffer.size(),
              (PEC_VARIANT)&buffer[0],
              &dwBufferSize)) {
          LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionRunTimeStatus");
        }
      } else {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionRunTimeStatus");
      }
    }

    vStatus = (PEC_VARIANT)&buffer[0];

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

    std::wstring eventSource = vProperty->StringVal;

    if (!getStatus(eventSource.c_str(), EcSubscriptionRunTimeStatusActive, 0, buffer, vProperty)) {
      return false;
    }

    if (vProperty->Type != EcVarTypeUInt32) {
      logInvalidSubscriptionPropertyType(__LINE__, vProperty->Type);
      return false;
    }

    auto runtimeStatus = vProperty->UInt32Val;

    std::wstring strRuntimeStatus;

    switch (vProperty->UInt32Val)
    {
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

    auto lastError = vProperty->UInt32Val;

    if (lastError == 0 && (runtimeStatus == EcRuntimeStatusActiveStatusActive || runtimeStatus == EcRuntimeStatusActiveStatusTrying)) {
      logger_->log_info("Subscription '%ws': status '%ws', no error.", s_subscriptionName.value().c_str(), strRuntimeStatus.c_str());
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
  if (!EcGetSubscriptionProperty(hSubscription, propID, flags, (DWORD)buffer.size(), (PEC_VARIANT)&buffer[0], &dwBufferSize)) {
    if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
      buffer.resize(dwBufferSize);
      if (!EcGetSubscriptionProperty(hSubscription, propID, flags, (DWORD)buffer.size(), (PEC_VARIANT)&buffer[0], &dwBufferSize)) {
        LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionProperty");
        return false;
      }
    } else {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcGetSubscriptionProperty");
      return false;
    }
  }

  vProperty = (PEC_VARIANT)&buffer[0];

  return true;
}

bool CollectorInitiatedSubscription::createSubscription(const std::shared_ptr<core::ProcessContext>& context) {

  s_supportedProperties.init(context);


  // If subcription already exists, delete it.
  EC_HANDLE hSubscription = EcOpenSubscription(s_subscriptionName.value().c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
  if (hSubscription) {
    EcClose(hSubscription);
    if (!EcDeleteSubscription(s_subscriptionName.value().c_str(), 0)) {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcDeleteSubscription");
      return false;
    }
  }

  // Create subscription.
  hSubscription = EcOpenSubscription(s_subscriptionName.value().c_str(), EC_READ_ACCESS | EC_WRITE_ACCESS, EC_CREATE_NEW);
  if (!hSubscription) {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("EcOpenSubscription");
    return false;
  }
  utils::ScopeGuard guard_hSubscription([hSubscription]() { EcClose(hSubscription); });

  struct SubscriptionProperty
  {
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

  std::vector<SubscriptionProperty> vProp = {
    {EcSubscriptionDescription, s_subscriptionDescription.value()},
    {EcSubscriptionURI, std::wstring(L"http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog")},
    {EcSubscriptionQuery, L"<QueryList><Query Path=\"" + s_sourceChannels.value() + L"\"><Select>*</Select></Query></QueryList>"},
    {EcSubscriptionLogFile, s_channel.value()},
    {EcSubscriptionConfigurationMode, (uint32_t)EcConfigurationModeCustom},
    {EcSubscriptionDeliveryMode, (uint32_t)EcDeliveryModePull},
    {EcSubscriptionDeliveryMaxItems, (uint32_t)s_maxDeliveryItems.value()},
    {EcSubscriptionDeliveryMaxLatencyTime, (uint32_t)s_deliveryMaxLatencyTime.value()},
    {EcSubscriptionHeartbeatInterval, (uint32_t)s_heartbeatInterval.value()},
    {EcSubscriptionContentFormat, (uint32_t)EcContentFormatRenderedText},
    {EcSubscriptionCredentialsType, (uint32_t)EcSubscriptionCredDefault},
    {EcSubscriptionEnabled, true},
    {EcSubscriptionCommonUserName, s_sourceUserName.value()},
    {EcSubscriptionCommonPassword, s_sourcePassword.value()}
  };
  for (auto prop : vProp) {
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

  EC_OBJECT_ARRAY_PROPERTY_HANDLE hArray = vProperty->PropertyHandleVal;
  utils::ScopeGuard guard_hArray([hArray]() { EcClose(hArray); });

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

  for (auto& prop: std::vector<SubscriptionProperty>{{EcSubscriptionEventSourceAddress, s_sourceAddress.value()}, {EcSubscriptionEventSourceEnabled, true}}) {
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

bool CollectorInitiatedSubscription::subscribe(const std::shared_ptr<core::ProcessContext> &context)
{
  logger_->log_debug("CollectorInitiatedSubscription: maxBufferSize_ %lld", s_maxBufferSize.value());

  provenanceUri_ = "winlog://" + computerName_ + "/" + to_string(s_channel.value()) + "?" + to_string(s_query.value());

  subscriptionHandle_ = EvtSubscribe(
      NULL,
      NULL,
      s_channel.value().c_str(),
      s_query.value().c_str(),
      NULL,
      this,
      [](EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) {
        auto pCollectorInitiatedSubscription = static_cast<CollectorInitiatedSubscription*>(pContext);

        auto& logger = pCollectorInitiatedSubscription->logger_;

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
              if (used > s_maxBufferSize.value()) {
                logger->log_error("Dropping event %x because it couldn't be rendered within %ll bytes.", hEvent, s_maxBufferSize.value());
                return 0UL;
              }

              size = used;
              std::vector<char> buf(size);
              if (EvtRender(NULL, hEvent, EvtRenderEventXml, size, &buf[0], &used, &propertyCount)) {
                std::string xml = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(reinterpret_cast<wchar_t*>(&buf[0]));

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

  lastActivityTimestamp_ = GetTickCount();

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
    session->transfer(flowFile, s_success);
    session->commit();

    flowFileCount++;
  }

  return flowFileCount;
}

void CollectorInitiatedSubscription::notifyStop() {
  if (!EcDeleteSubscription(s_subscriptionName.value().c_str(), 0)) {
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

  logger_->log_error("Line %d: '%s': error %d: %s\n", line, info.c_str(), (int)error, (char *)lpMsg);

  LocalFree(lpMsg);

  int z = 11;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
