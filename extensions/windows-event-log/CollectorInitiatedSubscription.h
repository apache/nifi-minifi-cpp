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
#include "SupportedProperty.h"

namespace org::apache::nifi::minifi::processors {

class CollectorInitiatedSubscription : public core::Processor {
 public:
  explicit CollectorInitiatedSubscription(const std::string& name, const utils::Identifier& uuid = {});
  virtual ~CollectorInitiatedSubscription() = default;

  EXTENSIONAPI static constexpr const char* Description = "Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.";
  static auto properties() { return std::array<core::Property, 0>{}; }
  static auto relationships() { return std::array<core::Relationship, 0>{}; }
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
  // Logger
  std::shared_ptr<core::logging::Logger> logger_;
  moodycamel::ConcurrentQueue<std::string> renderedXMLs_;
  std::string provenanceUri_;
  std::string computerName_;
  EVT_HANDLE subscriptionHandle_{};
  uint64_t lastActivityTimestamp_{};
  std::shared_ptr<core::ProcessSessionFactory> sessionFactory_;
  SupportedProperties supportedProperties_;
  SupportedProperty<std::wstring> subscriptionName_;
  SupportedProperty<std::wstring> subscriptionDescription_;
  SupportedProperty<std::wstring> sourceAddress_;
  SupportedProperty<std::wstring> sourceUserName_;
  SupportedProperty<std::wstring> sourcePassword_;
  SupportedProperty<std::wstring> sourceChannels_;
  SupportedProperty<uint64_t> maxDeliveryItems_;
  SupportedProperty<uint64_t> deliveryMaxLatencyTime_;
  SupportedProperty<uint64_t> heartbeatInterval_;
  SupportedProperty<std::wstring> channel_;
  SupportedProperty<std::wstring> query_;
  SupportedProperty<uint64_t> maxBufferSize_;
  SupportedProperty<uint64_t> inactiveDurationToReconnect_;
};

}  // namespace org::apache::nifi::minifi::processors
