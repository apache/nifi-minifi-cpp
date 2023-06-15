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

#pragma once

#include <Windows.h>
#include <winevt.h>
#include <EvColl.h>

#include <vector>
#include <string>
#include <memory>

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"

namespace org::apache::nifi::minifi::processors {

class CollectorInitiatedSubscription : public core::Processor {
 public:
  explicit CollectorInitiatedSubscription(const std::string& name, const utils::Identifier& uuid = {});
  virtual ~CollectorInitiatedSubscription() = default;

  EXTENSIONAPI static constexpr const char* Description = "Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.";

  EXTENSIONAPI static constexpr auto SubscriptionName = core::PropertyDefinitionBuilder<>::createProperty("Subscription Name")
      .isRequired(true)
      .withDescription("The name of the subscription. The value provided for this parameter should be unique within the computer's scope.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SubscriptionDescription = core::PropertyDefinitionBuilder<>::createProperty("Subscription Description")
      .isRequired(true)
      .withDescription("A description of the subscription.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SourceAddress = core::PropertyDefinitionBuilder<>::createProperty("Source Address")
      .isRequired(true)
      .withDescription("The IP address or fully qualified domain name (FQDN) of the local or remote computer (event source) from which the events are collected.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SourceUserName = core::PropertyDefinitionBuilder<>::createProperty("Source User Name")
      .isRequired(true)
      .withDescription("The user name, which is used by the remote computer (event source) to authenticate the user.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SourcePassword = core::PropertyDefinitionBuilder<>::createProperty("Source Password")
      .isRequired(true)
      .withDescription("The password, which is used by the remote computer (event source) to authenticate the user.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SourceChannels = core::PropertyDefinitionBuilder<>::createProperty("Source Channels")
      .isRequired(true)
      .withDescription("The Windows Event Log Channels (on domain computer(s)) from which events are transferred.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxDeliveryItems = core::PropertyDefinitionBuilder<>::createProperty("Max Delivery Items")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1000")
      .withDescription("Determines the maximum number of items that will forwarded from an event source for each request.")
      .build();
  EXTENSIONAPI static constexpr auto DeliveryMaxLatencyTime = core::PropertyDefinitionBuilder<>::createProperty("Delivery MaxLatency Time")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .withDescription("How long, in milliseconds, the event source should wait before sending events.")
      .build();
  EXTENSIONAPI static constexpr auto HeartbeatInterval = core::PropertyDefinitionBuilder<>::createProperty("Heartbeat Interval")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .withDescription(
          "Time interval, in milliseconds, which is observed between the sent heartbeat messages."
          " The event collector uses this property to determine the interval between queries to the event source.")
      .build();
  EXTENSIONAPI static constexpr auto Channel = core::PropertyDefinitionBuilder<>::createProperty("Channel")
      .isRequired(true)
      .withDefaultValue("ForwardedEvents")
      .withDescription("The Windows Event Log Channel (on local machine) to which events are transferred.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Query = core::PropertyDefinitionBuilder<>::createProperty("Query")
      .isRequired(true)
      .withDefaultValue("*")
      .withDescription("XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBufferSize = core::PropertyDefinitionBuilder<>::createProperty("Max Buffer Size")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1 MB")
      .withDescription(
          "The individual Event Log XMLs are rendered to a buffer."
          " This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)")
      .build();
  EXTENSIONAPI static constexpr auto InactiveDurationToReconnect = core::PropertyDefinitionBuilder<>::createProperty("Inactive Duration To Reconnect")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .withDescription(
          "If no new event logs are processed for the specified time period,"
          " this processor will try reconnecting to recover from a state where any further messages cannot be consumed."
          " Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned."
          " Setting no duration, e.g. '0 ms' disables auto-reconnection.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 13>{
      SubscriptionName,
      SubscriptionDescription,
      SourceAddress,
      SourceUserName,
      SourcePassword,
      SourceChannels,
      MaxDeliveryItems,
      DeliveryMaxLatencyTime,
      HeartbeatInterval,
      Channel,
      Query,
      MaxBufferSize,
      InactiveDurationToReconnect
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Relationship for successfully consumed events."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize(void) override;
  void notifyStop() override;

 protected:
  bool createSubscription(const std::shared_ptr<core::ProcessContext> &context);
  bool subscribe(const std::shared_ptr<core::ProcessContext> &context);
  void unsubscribe();
  int processQueue(const std::shared_ptr<core::ProcessSession> &session);
  void logError(int line, const std::string& error);
  void logWindowsError(int line, const std::string& info);
  void logInvalidSubscriptionPropertyType(int line, DWORD type);
  bool getSubscriptionProperty(EC_HANDLE hSubscription, EC_SUBSCRIPTION_PROPERTY_ID propID, DWORD flags, std::vector<BYTE>& buffer, PEC_VARIANT& vProperty);
  bool checkSubscriptionRuntimeStatus();

 private:
  std::shared_ptr<core::logging::Logger> logger_;
  moodycamel::ConcurrentQueue<std::string> renderedXMLs_;
  std::string provenanceUri_;
  std::string computerName_;
  EVT_HANDLE subscriptionHandle_{};
  uint64_t lastActivityTimestamp_{};
  std::shared_ptr<core::ProcessSessionFactory> sessionFactory_;
  std::wstring subscription_name_;
  core::DataSizeValue max_buffer_size_;
};

}  // namespace org::apache::nifi::minifi::processors
