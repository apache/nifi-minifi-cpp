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
#include "core/ProcessSessionFactory.h"
#include "core/Resource.h"
#include "utils/gsl.h"
#include "utils/OptionalUtils.h"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Wecapi.lib")

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

#define LOG_SUBSCRIPTION_ERROR(error) logError(__LINE__, error)
#define LOG_SUBSCRIPTION_WINDOWS_ERROR(info) logWindowsError(__LINE__, info)

namespace {
std::string to_string(const wchar_t *pChar) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pChar);
}

std::wstring to_wstring(const std::string& utf8_string) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(utf8_string);
}
}  // namespace

CollectorInitiatedSubscription::CollectorInitiatedSubscription(const std::string& name, const utils::Identifier& uuid)
  : core::Processor(name, uuid), logger_(core::logging::LoggerFactory<CollectorInitiatedSubscription>::getLogger(uuid_)) {
  char buff[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(buff);
  if (GetComputerName(buff, &size)) {
    computerName_ = buff;
  } else {
    LOG_SUBSCRIPTION_WINDOWS_ERROR("GetComputerName");
  }
}

void CollectorInitiatedSubscription::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void CollectorInitiatedSubscription::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  gsl_Expects(context);

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

  subscription_name_ = to_wstring(context->getProperty(SubscriptionName).value());
  max_buffer_size_ = context->getProperty<core::DataSizeValue>(MaxBufferSize).value();
}

void CollectorInitiatedSubscription::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context);

  if (!subscriptionHandle_) {
    if (!subscribe(context)) {
      context->yield();
      return;
    }
  }

  checkSubscriptionRuntimeStatus();

  const auto flowFileCount = processQueue(session);

  const auto now = GetTickCount64();

  if (flowFileCount > 0) {
    lastActivityTimestamp_ = now;
  } else if (auto inactive_duration_to_reconnect_ms = context->getProperty<core::TimePeriodValue>(InactiveDurationToReconnect)
             | utils::map([](const auto& time_period_value) { return time_period_value.getMilliseconds().count(); });
             inactive_duration_to_reconnect_ms && *inactive_duration_to_reconnect_ms > 0) {
    if ((now - lastActivityTimestamp_) > *inactive_duration_to_reconnect_ms) {
      logger_->log_info("Exceeds configured 'inactive duration to reconnect' %lld ms. Unsubscribe to reconnect..", *inactive_duration_to_reconnect_ms);
      unsubscribe();
    }
  }
}

void CollectorInitiatedSubscription::logInvalidSubscriptionPropertyType(int line, DWORD type) {
  logError(line, "Invalid property type: " + std::to_string(type));
}

bool CollectorInitiatedSubscription::checkSubscriptionRuntimeStatus() {
  EC_HANDLE hSubscription = EcOpenSubscription(subscription_name_.c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
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
        subscription_name_.c_str(),
          statusInfoID,
          eventSource.c_str(),
          flags,
          static_cast<DWORD>(buffer.size()),
          reinterpret_cast<PEC_VARIANT>(&buffer[0]),
          &dwBufferSize)) {
      if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
        buffer.resize(dwBufferSize);
        if (!EcGetSubscriptionRunTimeStatus(subscription_name_.c_str(),
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
      logger_->log_info("Subscription '%ws': status '%ws', no error.", subscription_name_.c_str(), strRuntimeStatus.c_str());
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
  gsl_Expects(context);

  // If subcription already exists, delete it.
  EC_HANDLE hSubscription = EcOpenSubscription(subscription_name_.c_str(), EC_READ_ACCESS, EC_OPEN_EXISTING);
  if (hSubscription) {
    EcClose(hSubscription);
    if (!EcDeleteSubscription(subscription_name_.c_str(), 0)) {
      LOG_SUBSCRIPTION_WINDOWS_ERROR("EcDeleteSubscription");
      return false;
    }
  }

  // Create subscription.
  hSubscription = EcOpenSubscription(subscription_name_.c_str(), EC_READ_ACCESS | EC_WRITE_ACCESS, EC_CREATE_NEW);
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

  const auto subscription_description = to_wstring(context->getProperty(SubscriptionDescription).value());
  const auto source_channels = to_wstring(context->getProperty(SourceChannels).value());
  const auto channel = to_wstring(context->getProperty(Channel).value());
  const auto max_delivery_items = context->getProperty<core::DataSizeValue>(MaxDeliveryItems).value().getValue();
  const auto delivery_max_latency_time = context->getProperty<core::TimePeriodValue>(DeliveryMaxLatencyTime).value().getMilliseconds().count();
  const auto heartbeat_interval = context->getProperty<core::TimePeriodValue>(HeartbeatInterval).value().getMilliseconds().count();
  const auto source_user_name = to_wstring(context->getProperty(SourceUserName).value());
  const auto source_password = to_wstring(context->getProperty(SourcePassword).value());

  std::vector<SubscriptionProperty> listProperty = {
    {EcSubscriptionDescription, subscription_description},
    {EcSubscriptionURI, std::wstring(L"http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog")},
    {EcSubscriptionQuery, L"<QueryList><Query Path=\"" + source_channels + L"\"><Select>*</Select></Query></QueryList>"},
    {EcSubscriptionLogFile, channel},
    {EcSubscriptionConfigurationMode, static_cast<uint32_t>(EcConfigurationModeCustom)},
    {EcSubscriptionDeliveryMode, static_cast<uint32_t>(EcDeliveryModePull)},
    {EcSubscriptionDeliveryMaxItems, static_cast<uint32_t>(max_delivery_items)},
    {EcSubscriptionDeliveryMaxLatencyTime, static_cast<uint32_t>(delivery_max_latency_time)},
    {EcSubscriptionHeartbeatInterval, static_cast<uint32_t>(heartbeat_interval)},
    {EcSubscriptionContentFormat, static_cast<uint32_t>(EcContentFormatRenderedText)},
    {EcSubscriptionCredentialsType, static_cast<uint32_t>(EcSubscriptionCredDefault)},
    {EcSubscriptionEnabled, true},
    {EcSubscriptionCommonUserName, source_user_name},
    {EcSubscriptionCommonPassword, source_password}
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

  const auto source_address = to_wstring(context->getProperty(SourceAddress).value());
  for (auto& prop : std::vector<SubscriptionProperty>{{EcSubscriptionEventSourceAddress, source_address}, {EcSubscriptionEventSourceEnabled, true}}) {
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
  gsl_Expects(context);

  logger_->log_debug("CollectorInitiatedSubscription: MaxBufferSize %lld", max_buffer_size_.getValue());

  const auto channel = context->getProperty(Channel).value();
  const auto query = context->getProperty(Query).value();
  provenanceUri_ = "winlog://" + computerName_ + "/" + channel + "?" + query;

  const auto channel_ws = to_wstring(context->getProperty(Channel).value());
  const auto query_ws = to_wstring(context->getProperty(Query).value());

  const EVT_SUBSCRIBE_CALLBACK callback = [](EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) {
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
          if (used > pCollectorInitiatedSubscription->max_buffer_size_.getValue()) {
            logger->log_error("Dropping event %p because it couldn't be rendered within %llu bytes.", hEvent, pCollectorInitiatedSubscription->max_buffer_size_.getValue());
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
  };

  subscriptionHandle_ = EvtSubscribe(
      NULL,
      NULL,
      channel_ws.c_str(),
      query_ws.c_str(),
      NULL,
      this,
      callback,
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
  int flowFileCount = 0;

  std::string xml;
  while (renderedXMLs_.try_dequeue(xml)) {
    auto flowFile = session->create();

    session->writeBuffer(flowFile, xml);
    session->putAttribute(flowFile, core::SpecialFlowAttribute::MIME_TYPE, "application/xml");
    session->getProvenanceReporter()->receive(flowFile, provenanceUri_, getUUIDStr(), "Consume windows event logs", 0ms);
    session->transfer(flowFile, Success);

    flowFileCount++;
  }

  return flowFileCount;
}

void CollectorInitiatedSubscription::notifyStop() {
  if (!EcDeleteSubscription(subscription_name_.c_str(), 0)) {
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

REGISTER_RESOURCE(CollectorInitiatedSubscription, Processor);

}  // namespace org::apache::nifi::minifi::processors
