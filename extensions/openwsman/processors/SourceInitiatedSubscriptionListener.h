/**
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

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include <CivetServer.h>
extern "C" {
#include "wsman-xml.h"
}

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::processors {

class SourceInitiatedSubscriptionListener : public core::Processor {
 public:
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_NONE = "None";
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_ALL = "All";

  explicit SourceInitiatedSubscriptionListener(std::string name, const utils::Identifier& uuid = {});

  EXTENSIONAPI static constexpr const char* Description = "This processor implements a Windows Event Forwarding Source Initiated Subscription server with the help of OpenWSMAN. "
      "Windows hosts can be set up to connect and forward Event Logs to this processor.";

  EXTENSIONAPI static const core::Property ListenHostname;
  EXTENSIONAPI static const core::Property ListenPort;
  EXTENSIONAPI static const core::Property SubscriptionManagerPath;
  EXTENSIONAPI static const core::Property SubscriptionsBasePath;
  EXTENSIONAPI static const core::Property SSLCertificate;
  EXTENSIONAPI static const core::Property SSLCertificateAuthority;
  EXTENSIONAPI static const core::Property SSLVerifyPeer;
  EXTENSIONAPI static const core::Property XPathXmlQuery;
  EXTENSIONAPI static const core::Property InitialExistingEventsStrategy;
  EXTENSIONAPI static const core::Property SubscriptionExpirationInterval;
  EXTENSIONAPI static const core::Property HeartbeatInterval;
  EXTENSIONAPI static const core::Property MaxElements;
  EXTENSIONAPI static const core::Property MaxLatency;
  EXTENSIONAPI static const core::Property ConnectionRetryInterval;
  EXTENSIONAPI static const core::Property ConnectionRetryCount;
  static auto properties() {
    return std::array{
      ListenHostname,
      ListenPort,
      SubscriptionManagerPath,
      SubscriptionsBasePath,
      SSLCertificate,
      SSLCertificateAuthority,
      SSLVerifyPeer,
      XPathXmlQuery,
      InitialExistingEventsStrategy,
      SubscriptionExpirationInterval,
      HeartbeatInterval,
      MaxElements,
      MaxLatency,
      ConnectionRetryInterval,
      ConnectionRetryCount
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_WEF_REMOTE_MACHINEID = "wef.remote.machineid";
  static constexpr char const* ATTRIBUTE_WEF_REMOTE_IP = "wef.remote.ip";

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void notifyStop() override;

  class Handler: public CivetHandler {
   public:
    explicit Handler(SourceInitiatedSubscriptionListener& processor);
    bool handlePost(CivetServer* server, struct mg_connection* conn) override;

   private:
    SourceInitiatedSubscriptionListener& processor_;

    bool handleSubscriptionManager(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);
    bool handleSubscriptions(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);

    static int enumerateEventCallback(WsXmlNodeH node, void* data);
    static std::string getSoapAction(WsXmlDocH doc);
    static std::string getMachineId(WsXmlDocH doc);
    static bool isAckRequested(WsXmlDocH doc);
    void sendResponse(struct mg_connection* conn, const std::string& machineId, const std::string& remoteIp, char* xml_buf, size_t xml_buf_size);

    static std::string millisecondsToXsdDuration(std::chrono::milliseconds milliseconds);
  };

 protected:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SourceInitiatedSubscriptionListener>::getLogger();

  core::StateManager* state_manager_;

  std::shared_ptr<core::ProcessSessionFactory> session_factory_;

  std::string listen_hostname_;
  uint16_t listen_port_;
  std::string subscription_manager_path_;
  std::string subscriptions_base_path_;
  std::string ssl_ca_cert_thumbprint_;
  std::string xpath_xml_query_;
  std::string initial_existing_events_strategy_;
  std::chrono::milliseconds subscription_expiration_interval_;
  std::chrono::milliseconds heartbeat_interval_;
  uint32_t max_elements_;
  std::chrono::milliseconds max_latency_;
  std::chrono::milliseconds connection_retry_interval_;
  uint32_t connection_retry_count_;

  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;

  struct SubscriberData {
      WsXmlDocH bookmark_;
      std::string subscription_version_;
      WsXmlDocH subscription_;
      std::string subscription_endpoint_;
      std::string subscription_identifier_;

      SubscriberData();
      ~SubscriberData();

      void setSubscription(const std::string& subscription_version, WsXmlDocH subscription, const std::string& subscription_endpoint, const std::string& subscription_identifier);
      void clearSubscription();
      void setBookmark(WsXmlDocH bookmark);
      void clearBookmark();
  };

  std::mutex mutex_;
  std::map<std::string /*machineId*/, SubscriberData> subscribers_;

  bool persistState() const;
  bool loadState();
};

}  // namespace org::apache::nifi::minifi::processors
