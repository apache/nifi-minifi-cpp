/**
 * @file CollectorInitiatedSubscription.h
 * CollectorInitiatedSubscription class declaration
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


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! CollectorInitiatedSubscription Class
class CollectorInitiatedSubscription : public core::Processor {
 public:
  //! Constructor
  /*!
  * Create a new processor
  */
  explicit CollectorInitiatedSubscription(const std::string& name, const utils::Identifier& uuid = {});

  //! Destructor
  virtual ~CollectorInitiatedSubscription() = default;

  //! Processor Name
  static const std::string ProcessorName;

 public:
  /**
  * Function that's executed when the processor is scheduled.
  * @param context process context.
  * @param sessionFactory process session factory that is used when creating
  * ProcessSession objects.
  */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  //! OnTrigger method, implemented by NiFi CollectorInitiatedSubscription
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  //! Initialize, overwrite by NiFi CollectorInitiatedSubscription
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
  std::shared_ptr<logging::Logger> logger_;
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
