/**
 * @file RemoteProcessGroupPort.h
 * RemoteProcessGroupPort class declaration
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

#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include <memory>
#include <stack>

#include "http/BaseHTTPClient.h"
#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "sitetosite/SiteToSiteClient.h"
#include "minifi-cpp/controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"
#include "core/ClassLoader.h"

namespace org::apache::nifi::minifi {

/**
 * Count down latch implementation that's used across
 * all threads of the RPG. This is okay since the latch increments
 * and decrements based on its construction. Using RAII we should
 * never have the concern of thread safety.
 */
class RPGLatch {
 public:
  RPGLatch(bool increment = true) { // NOLINT
    static std::atomic<int64_t> latch_count(0);
    count = &latch_count;
    if (increment)
      count++;
  }

  ~RPGLatch() {
    count--;
  }

  int64_t getCount() {
    return *count;
  }

 private:
  std::atomic<int64_t> *count;
};

struct RPG {
  std::string host;
  int port;
  std::string protocol;
};

class RemoteProcessGroupPort : public core::ProcessorImpl {
 public:
  RemoteProcessGroupPort(std::string_view name, std::string url, std::shared_ptr<Configure> configure, const utils::Identifier &uuid = {})
      : core::ProcessorImpl(name, uuid),
        configure_(std::move(configure)),
        direction_(sitetosite::TransferDirection::SEND),
        transmitting_(false),
        protocol_uuid_(uuid),
        client_type_(sitetosite::ClientType::RAW),
        peer_index_(-1),
        logger_(core::logging::LoggerFactory<RemoteProcessGroupPort>::getLogger(uuid)) {
    // REST API port and host
    setURL(std::move(url));
  }
  virtual ~RemoteProcessGroupPort() = default;

  MINIFIAPI static constexpr auto hostName = core::PropertyDefinitionBuilder<>::createProperty("Host Name")
      .withDescription("Remote Host Name.")
      .build();
  MINIFIAPI static constexpr auto SSLContext = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.")
      .build();
  MINIFIAPI static constexpr auto port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("Remote Port")
      .build();
  MINIFIAPI static constexpr auto portUUID = core::PropertyDefinitionBuilder<>::createProperty("Port UUID")
      .withDescription("Specifies remote NiFi Port UUID.")
      .build();
  MINIFIAPI static constexpr auto idleTimeout = core::PropertyDefinitionBuilder<>::createProperty("Idle Timeout")
    .withDescription("Max idle time for remote service")
    .isRequired(true)
    .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
    .withDefaultValue("15 s")
    .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      hostName,
      SSLContext,
      port,
      portUUID,
      idleTimeout
  });

  MINIFIAPI static constexpr auto DefaultRelationship = core::RelationshipDefinition{"undefined", ""};
  MINIFIAPI static constexpr auto Relationships = std::array{DefaultRelationship};

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;
  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  MINIFIAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  void setDirection(sitetosite::TransferDirection direction) {
    direction_ = direction;
    if (direction_ == sitetosite::TransferDirection::RECEIVE) {
      setTriggerWhenEmpty(true);
    } else {
      setTriggerWhenEmpty(false);
    }
  }

  void setTimeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
  }

  [[nodiscard]] std::optional<std::chrono::milliseconds> getTimeout() const {
    return timeout_;
  }

  void setTransmitting(bool val) {
    transmitting_ = val;
  }

  void setInterface(const std::string &ifc) {
    local_network_interface_ = ifc;
  }

  void setURL(const std::string& val);

  [[nodiscard]] std::vector<RPG> getInstances() const {
    return nifi_instances_;
  }

  void setHTTPProxy(const http::HTTPProxy &proxy) {
    proxy_ = proxy;
  }

  [[nodiscard]] http::HTTPProxy getHTTPProxy() const {
    return proxy_;
  }

  void notifyStop() override;

  void enableHTTP() {
    client_type_ = sitetosite::ClientType::HTTP;
  }

  void setUseCompression(bool use_compression) {
    use_compression_ = use_compression;
  }

  [[nodiscard]] bool getUseCompression() const {
    return use_compression_;
  }

  void setBatchCount(uint64_t count) {
    batch_count_ = count;
  }

  [[nodiscard]] std::optional<uint64_t> getBatchCount() const {
    return batch_count_;
  }

  void setBatchSize(uint64_t size) {
    batch_size_ = size;
  }

  [[nodiscard]] std::optional<uint64_t> getBatchSize() const {
    return batch_size_;
  }

  void setBatchDuration(std::chrono::milliseconds duration) {
    batch_duration_ = duration;
  }

  [[nodiscard]] std::optional<std::chrono::milliseconds> getBatchDuration() const {
    return batch_duration_;
  }

 protected:
  std::optional<std::pair<std::string, uint16_t>> refreshRemoteSiteToSiteInfo();
  void refreshPeerList();
  std::unique_ptr<sitetosite::SiteToSiteClient> getNextProtocol();
  void returnProtocol(std::unique_ptr<sitetosite::SiteToSiteClient> protocol);

  moodycamel::ConcurrentQueue<std::unique_ptr<sitetosite::SiteToSiteClient>> available_protocols_;
  std::shared_ptr<Configure> configure_;
  sitetosite::TransferDirection direction_;
  std::atomic<bool> transmitting_;
  std::optional<std::chrono::milliseconds> timeout_;
  std::string local_network_interface_;
  utils::Identifier protocol_uuid_;
  std::chrono::milliseconds idle_timeout_ = 15s;
  std::vector<RPG> nifi_instances_;
  http::HTTPProxy proxy_;
  sitetosite::ClientType client_type_;
  std::vector<sitetosite::PeerStatus> peers_;
  std::atomic<int64_t> peer_index_;
  std::mutex peer_mutex_;
  std::shared_ptr<controllers::SSLContextService> ssl_service_;
  bool use_compression_{false};
  std::optional<uint64_t> batch_count_;
  std::optional<uint64_t> batch_size_;
  std::optional<std::chrono::milliseconds> batch_duration_;

 private:
  std::unique_ptr<sitetosite::SiteToSiteClient> initializeProtocol(sitetosite::SiteToSiteClientConfiguration& config) const;
  [[nodiscard]] std::optional<std::string> getRestApiToken(const RPG& nifi) const;
  std::optional<std::pair<std::string, uint16_t>> parseSiteToSiteDataFromControllerConfig(const RPG& nifi, const std::string& controller) const;
  std::optional<std::pair<std::string, uint16_t>> tryRefreshSiteToSiteInstance(RPG nifi) const;

  std::shared_ptr<core::logging::Logger> logger_;
  static const char* RPG_SSL_CONTEXT_SERVICE_NAME;
};

}  // namespace org::apache::nifi::minifi
