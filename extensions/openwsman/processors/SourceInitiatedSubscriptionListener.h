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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class SourceInitiatedSubscriptionListener : public core::Processor {
 public:
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_NONE = "None";
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_ALL = "All";

  static constexpr char const* ProcessorName = "SourceInitiatedSubscriptionListener";

  explicit SourceInitiatedSubscriptionListener(const std::string& name, const utils::Identifier& uuid = {});

  // Supported Properties
  static core::Property ListenHostname;
  static core::Property ListenPort;
  static core::Property SubscriptionManagerPath;
  static core::Property SubscriptionsBasePath;
  static core::Property SSLCertificate;
  static core::Property SSLCertificateAuthority;
  static core::Property SSLVerifyPeer;
  static core::Property XPathXmlQuery;
  static core::Property InitialExistingEventsStrategy;
  static core::Property SubscriptionExpirationInterval;
  static core::Property HeartbeatInterval;
  static core::Property MaxElements;
  static core::Property MaxLatency;
  static core::Property ConnectionRetryInterval;
  static core::Property ConnectionRetryCount;

  // Supported Relationships
  static core::Relationship Success;

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
    bool handlePost(CivetServer* server, struct mg_connection* conn);

    class WriteCallback : public OutputStreamCallback {
     public:
      explicit WriteCallback(char* text);
      int64_t process(const std::shared_ptr<io::BaseStream>& stream);

     private:
      char* text_;
    };

   private:
    SourceInitiatedSubscriptionListener& processor_;

    bool handleSubscriptionManager(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);
    bool handleSubscriptions(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);

    static int enumerateEventCallback(WsXmlNodeH node, void* data);
    std::string getSoapAction(WsXmlDocH doc);
    std::string getMachineId(WsXmlDocH doc);
    bool isAckRequested(WsXmlDocH doc);
    void sendResponse(struct mg_connection* conn, const std::string& machineId, const std::string& remoteIp, char* xml_buf, size_t xml_buf_size);

    static std::string millisecondsToXsdDuration(int64_t milliseconds);
  };

 protected:
  std::shared_ptr<logging::Logger> logger_;

  std::shared_ptr<core::CoreComponentStateManager> state_manager_;

  std::shared_ptr<core::ProcessSessionFactory> session_factory_;

  std::string listen_hostname_;
  uint16_t listen_port_;
  std::string subscription_manager_path_;
  std::string subscriptions_base_path_;
  std::string ssl_ca_cert_thumbprint_;
  std::string xpath_xml_query_;
  std::string initial_existing_events_strategy_;
  int64_t subscription_expiration_interval_;
  int64_t heartbeat_interval_;
  uint32_t max_elements_;
  int64_t max_latency_;
  int64_t connection_retry_interval_;
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
