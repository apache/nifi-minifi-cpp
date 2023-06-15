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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
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

  EXTENSIONAPI static constexpr auto ListenHostname = core::PropertyDefinitionBuilder<>::createProperty("Listen Hostname")
      .withDescription("The hostname or IP of this machine that will be advertised to event sources to connect to. "
          "It must be contained as a Subject Alternative Name in the server certificate, "
          "otherwise source machines will refuse to connect.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ListenPort = core::PropertyDefinitionBuilder<>::createProperty("Listen Port")
      .withDescription("The port to listen on.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .withDefaultValue("5986")
      .build();
  EXTENSIONAPI static constexpr auto SubscriptionManagerPath = core::PropertyDefinitionBuilder<>::createProperty("Subscription Manager Path")
      .withDescription("The URI path that will be used for the WEC Subscription Manager endpoint.")
      .isRequired(true)
      .withDefaultValue("/wsman/SubscriptionManager/WEC")
      .build();
  EXTENSIONAPI static constexpr auto SubscriptionsBasePath = core::PropertyDefinitionBuilder<>::createProperty("Subscriptions Base Path")
      .withDescription("The URI path that will be used as the base for endpoints serving individual subscriptions.")
      .isRequired(true)
      .withDefaultValue("/wsman/subscriptions")
      .build();
  EXTENSIONAPI static constexpr auto SSLCertificate = core::PropertyDefinitionBuilder<>::createProperty("SSL Certificate")
      .withDescription("File containing PEM-formatted file including TLS/SSL certificate and key. "
          "The root CA of the certificate must be the CA set in SSL Certificate Authority.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLCertificateAuthority = core::PropertyDefinitionBuilder<>::createProperty("SSL Certificate Authority")
      .withDescription("File containing the PEM-formatted CA that is the root CA for both this server's certificate and the event source clients' certificates.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLVerifyPeer = core::PropertyDefinitionBuilder<>::createProperty("SSL Verify Peer")
      .withDescription("Whether or not to verify the client's certificate")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto XPathXmlQuery = core::PropertyDefinitionBuilder<>::createProperty("XPath XML Query")
      .withDescription("An XPath Query in structured XML format conforming to the Query Schema described in "
          "https://docs.microsoft.com/en-gb/windows/win32/wes/queryschema-schema, "
          "see an example here: https://docs.microsoft.com/en-gb/windows/win32/wes/consuming-events")
      .isRequired(true)
      .withDefaultValue("<QueryList>\n"
          "  <Query Id=\"0\">\n"
          "    <Select Path=\"Application\">*</Select>\n"
          "  </Query>\n"
          "</QueryList>\n")
       .build();
  EXTENSIONAPI static constexpr auto InitialExistingEventsStrategy = core::PropertyDefinitionBuilder<2>::createProperty("Initial Existing Events Strategy")
      .withDescription("Defines the behaviour of the Processor when a new event source connects.\n"
          "None: will not request existing events\n"
          "All: will request all existing events matching the query")
      .isRequired(true)
      .withAllowedValues({INITIAL_EXISTING_EVENTS_STRATEGY_NONE, INITIAL_EXISTING_EVENTS_STRATEGY_ALL})
      .withDefaultValue(INITIAL_EXISTING_EVENTS_STRATEGY_NONE)
      .build();
  EXTENSIONAPI static constexpr auto SubscriptionExpirationInterval = core::PropertyDefinitionBuilder<>::createProperty("Subscription Expiration Interval")
      .withDescription("The interval while a subscription is valid without renewal. "
          "Because in a source-initiated subscription, the collector can not cancel the subscription, "
          "setting this too large could cause unnecessary load on the source machine. "
          "Setting this too small causes frequent reenumeration and resubscription which is ineffective.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .build();
  EXTENSIONAPI static constexpr auto HeartbeatInterval = core::PropertyDefinitionBuilder<>::createProperty("Heartbeat Interval")
      .withDescription("The processor will ask the sources to send heartbeats with this interval.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("30 sec")
      .build();
  EXTENSIONAPI static constexpr auto MaxElements = core::PropertyDefinitionBuilder<>::createProperty("Max Elements")
      .withDescription("The maximum number of events a source will batch together and send at once.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("20")
      .build();
  EXTENSIONAPI static constexpr auto MaxLatency = core::PropertyDefinitionBuilder<>::createProperty("Max Latency")
      .withDescription("The maximum time a source will wait to send new events.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 sec")
      .build();
  EXTENSIONAPI static constexpr auto ConnectionRetryInterval = core::PropertyDefinitionBuilder<>::createProperty("Connection Retry Interval")
      .withDescription("The interval with which a source will try to reconnect to the server.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 sec")
      .build();
  EXTENSIONAPI static constexpr auto ConnectionRetryCount = core::PropertyDefinitionBuilder<>::createProperty("Connection Retry Count")
      .withDescription("The number of connection retries after which a source will consider the subscription expired.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("5")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 15>{
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


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All Events are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

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
