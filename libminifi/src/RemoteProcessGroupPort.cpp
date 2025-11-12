/**
 * @file RemoteProcessGroupPort.cpp
 * RemoteProcessGroupPort class implementation
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

#include "RemoteProcessGroupPort.h"

#include <cinttypes>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "minifi-cpp/Exception.h"
#include "controllers/SSLContextService.h"
#include "core/controller/ControllerService.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Processor.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "http/BaseHTTPClient.h"
#include "rapidjson/document.h"
#include "sitetosite/Peer.h"
#include "sitetosite/SiteToSiteFactory.h"
#include "utils/net/DNS.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

namespace {
std::string buildFullSiteToSiteUrl(const RPG& nifi) {
  std::stringstream full_url;
  full_url << nifi.protocol << nifi.host;
  // don't append port if it is 0 ( undefined )
  if (nifi.port > 0) {
    full_url << ":" << std::to_string(nifi.port);
  }
  full_url << "/nifi-api/site-to-site";
  return full_url.str();
}
}  // namespace

const char *RemoteProcessGroupPort::RPG_SSL_CONTEXT_SERVICE_NAME = "RemoteProcessGroupPortSSLContextService";

void RemoteProcessGroupPort::setURL(const std::string& val) {
  auto urls = utils::string::split(val, ",");
  for (const auto& url : urls) {
    http::URL parsed_url{utils::string::trim(url)};
    if (parsed_url.isValid()) {
      logger_->log_debug("Parsed RPG URL '{}' -> '{}'", url, parsed_url.hostPort());
      nifi_instances_.push_back({parsed_url.host(), parsed_url.port(), parsed_url.protocol()});
    } else {
      logger_->log_error("Could not parse RPG URL '{}'", url);
    }
  }
}

gsl::not_null<std::unique_ptr<sitetosite::SiteToSiteClient>> RemoteProcessGroupPort::initializeProtocol(sitetosite::SiteToSiteClientConfiguration& config) const {
  config.setSecurityContext(ssl_service_);
  config.setHTTPProxy(proxy_);
  config.setIdleTimeout(idle_timeout_);
  config.setUseCompression(use_compression_);
  config.setBatchCount(batch_count_);
  config.setBatchSize(batch_size_);
  config.setBatchDuration(batch_duration_);
  config.setTimeout(timeout_);

  return sitetosite::createClient(config);
}

std::unique_ptr<sitetosite::SiteToSiteClient> RemoteProcessGroupPort::getNextProtocol() {
  std::unique_ptr<sitetosite::SiteToSiteClient> next_protocol = nullptr;
  if (!available_protocols_.try_dequeue(next_protocol)) {
    std::lock_guard<std::mutex> lock(peer_mutex_);
    if (peer_index_ >= 0) {
      logger_->log_debug("Creating client from peer {}", peer_index_);
      auto& peer_status = peers_[peer_index_];
      sitetosite::SiteToSiteClientConfiguration config(peer_status.getPortId(), peer_status.getHost(), peer_status.getPort(), local_network_interface_, client_type_);
      peer_index_++;
      if (peer_index_ >= gsl::narrow<int>(peers_.size())) {
        peer_index_ = 0;
      }
      next_protocol = initializeProtocol(config);
    } else {
      logger_->log_debug("Refreshing the peer list since there are none configured.");
      refreshPeerList();
    }
  }
  logger_->log_debug("Obtained protocol from available_protocols_");
  return next_protocol;
}

void RemoteProcessGroupPort::returnProtocol(core::ProcessContext& context, std::unique_ptr<sitetosite::SiteToSiteClient> return_protocol) {
  auto count = std::max<size_t>(context.getProcessor().getMaxConcurrentTasks(), peers_.size());
  if (available_protocols_.size_approx() >= count) {
    logger_->log_debug("not enqueueing protocol {}", getUUIDStr());
    // let the memory be freed
    return;
  }
  logger_->log_debug("enqueueing protocol {}, have a total of {}", getUUIDStr(), available_protocols_.size_approx());
  available_protocols_.enqueue(std::move(return_protocol));
}

void RemoteProcessGroupPort::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);

  logger_->log_trace("Finished initialization");
}

void RemoteProcessGroupPort::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  if (auto protocol_uuid = context.getProperty(portUUID)) {
    protocol_uuid_ = *protocol_uuid;
  }

  auto context_name = context.getProperty(SSLContext);
  if (!context_name || IsNullOrEmpty(*context_name)) {
    context_name = RPG_SSL_CONTEXT_SERVICE_NAME;
  }

  std::shared_ptr<core::controller::ControllerServiceInterface> service = context.getControllerService(*context_name, getUUID());
  if (nullptr != service) {
    ssl_service_ = std::dynamic_pointer_cast<minifi::controllers::SSLContextServiceInterface>(service);
  } else {
    std::string secureStr;
    if (configure_->get(Configure::nifi_remote_input_secure, secureStr) && utils::string::toBool(secureStr).value_or(false)) {
      ssl_service_ = controllers::SSLContextService::createAndEnable(RPG_SSL_CONTEXT_SERVICE_NAME, configure_);
    }
  }

  idle_timeout_ = context.getProperty(idleTimeout) | utils::andThen(parsing::parseDuration<std::chrono::milliseconds>) | utils::orThrow("RemoteProcessGroupPort::idleTimeout is a required Property");

  std::lock_guard<std::mutex> lock(peer_mutex_);
  if (!nifi_instances_.empty()) {
    refreshPeerList();
    if (!peers_.empty())
      peer_index_ = 0;
  }
  // populate the site2site protocol for load balancing between them
  if (!peers_.empty()) {
    auto count = std::max<size_t>(context.getProcessor().getMaxConcurrentTasks(), peers_.size());
    for (uint32_t i = 0; i < count; i++) {
      auto peer_status = peers_[peer_index_];
      sitetosite::SiteToSiteClientConfiguration config(peer_status.getPortId(), peer_status.getHost(), peer_status.getPort(), local_network_interface_, client_type_);
      peer_index_++;
      if (peer_index_ >= gsl::narrow<int>(peers_.size())) {
        peer_index_ = 0;
      }
      logger_->log_trace("Creating client");
      auto next_protocol = initializeProtocol(config);
      logger_->log_trace("Created client, moving into available protocols");
      returnProtocol(context, std::move(next_protocol));
    }
  } else {
    // we don't have any peers
    logger_->log_error("No peers selected during scheduling");
  }
}

void RemoteProcessGroupPort::notifyStop() {
  transmitting_ = false;
  std::unique_ptr<sitetosite::SiteToSiteClient> next_protocol = nullptr;
  while (available_protocols_.try_dequeue(next_protocol)) {
    // clear all protocols now
  }
}

void RemoteProcessGroupPort::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("On trigger {}", getUUIDStr());
  if (!transmitting_) {
    return;
  }

  try {
    logger_->log_trace("get protocol in on trigger");
    auto protocol = getNextProtocol();

    if (!protocol) {
      logger_->log_info("no protocol, yielding");
      context.yield();
      return;
    }

    if (!protocol->transfer(direction_, context, session)) {
      logger_->log_warn("protocol transmission failed, yielding");
      context.yield();
    }

    returnProtocol(context, std::move(protocol));
  } catch (const std::exception&) {
    context.yield();
    session.rollback();
  }
}

std::optional<std::string> RemoteProcessGroupPort::getRestApiToken(const RPG& nifi) const {
  std::string rest_user_name;
  configure_->get(Configure::nifi_rest_api_user_name, rest_user_name);
  if (rest_user_name.empty()) {
    return std::nullopt;
  }

  std::string rest_password;
  configure_->get(Configure::nifi_rest_api_password, rest_password);

  std::stringstream login_url;
  login_url << nifi.protocol << nifi.host;
  // don't append port if it is 0 ( undefined )
  if (nifi.port > 0) {
    login_url << ":" << std::to_string(nifi.port);
  }
  login_url << "/nifi-api/access/token";

  auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
  if (nullptr == client_ptr) {
    logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
    return std::nullopt;
  }
  auto client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
  client->initialize(http::HttpRequestMethod::Get, login_url.str(), ssl_service_);
  // use a connection timeout. if this times out we will simply attempt re-connection
  // so no need for configuration parameter that isn't already defined in Processor
  client->setConnectionTimeout(10s);
  client->setReadTimeout(idle_timeout_);

  auto token = http::get_token(client.get(), rest_user_name, rest_password);
  logger_->log_debug("Got token from NiFi REST Api endpoint {}", login_url.str());
  return token;
}

std::optional<std::pair<std::string, uint16_t>> RemoteProcessGroupPort::parseSiteToSiteDataFromControllerConfig(const RPG& nifi, const std::string& controller) const {
  rapidjson::Document doc;
  rapidjson::ParseResult ok = doc.Parse(controller.c_str());

  if (!ok || !doc.IsObject() || doc.ObjectEmpty()) {
    return std::nullopt;
  }

  rapidjson::Value::MemberIterator itr = doc.FindMember("controller");

  if (itr == doc.MemberEnd() || !itr->value.IsObject()) {
    return std::nullopt;
  }

  rapidjson::Value controllerValue = itr->value.GetObject();
  rapidjson::Value::ConstMemberIterator end_itr = controllerValue.MemberEnd();
  rapidjson::Value::ConstMemberIterator port_itr = controllerValue.FindMember("remoteSiteListeningPort");
  rapidjson::Value::ConstMemberIterator secure_itr = controllerValue.FindMember("siteToSiteSecure");
  bool site_to_site_secure = secure_itr != end_itr && secure_itr->value.IsBool() ? secure_itr->value.GetBool() : false;

  uint16_t site_to_site_port = 0;
  if (client_type_ == sitetosite::ClientType::RAW && port_itr != end_itr && port_itr->value.IsNumber()) {
    site_to_site_port = port_itr->value.GetInt();
  } else {
    site_to_site_port = nifi.port;
  }

  logger_->log_debug("process group remote site2site port {}, is secure {}", site_to_site_port, site_to_site_secure);
  return std::make_pair(nifi.host, site_to_site_port);
}

std::optional<std::pair<std::string, uint16_t>> RemoteProcessGroupPort::tryRefreshSiteToSiteInstance(RPG nifi) const {  // NOLINT(performance-unnecessary-value-param)
#ifdef WIN32
  if ("localhost" == nifi.host) {
    nifi.host = org::apache::nifi::minifi::utils::net::getMyHostName();
  }
#endif
  auto token = getRestApiToken(nifi);
  if (token && token->empty()) {
    return std::nullopt;
  }

  auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
  if (nullptr == client_ptr) {
    logger_->log_error("Could not locate HTTPClient. You do not have cURL support, defaulting to base configuration!");
    return std::nullopt;
  }

  auto client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
  auto full_url  = buildFullSiteToSiteUrl(nifi);
  client->initialize(http::HttpRequestMethod::Get, full_url, ssl_service_);
  // use a connection timeout. if this times out we will simply attempt re-connection
  // so no need for configuration parameter that isn't already defined in Processor
  client->setConnectionTimeout(10s);
  client->setReadTimeout(idle_timeout_);
  if (!proxy_.host.empty()) {
    client->setHTTPProxy(proxy_);
  }
  if (token) {
    client->setRequestHeader("Authorization", token);
  }

  client->setVerbose(false);

  if (!client->submit() || client->getResponseCode() != 200) {
    logger_->log_error("ProcessGroup::refreshRemoteSiteToSiteInfo -- curl_easy_perform() failed , response code {}\n", client->getResponseCode());
    return std::nullopt;
  }

  const std::vector<char> &response_body = client->getResponseBody();
  if (response_body.empty()) {
    logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSiteToSiteInfo: received HTTP code {} from {}", client->getResponseCode(), full_url);
    return std::nullopt;
  }

  std::string controller = std::string(response_body.begin(), response_body.end());
  logger_->log_trace("controller config {}", controller);
  return parseSiteToSiteDataFromControllerConfig(nifi, controller);
}

std::optional<std::pair<std::string, uint16_t>> RemoteProcessGroupPort::refreshRemoteSiteToSiteInfo() {
  if (nifi_instances_.empty()) {
    return std::nullopt;
  }

  for (const auto& nifi : nifi_instances_) {
    auto result = tryRefreshSiteToSiteInstance(nifi);
    if (result) {
      return result;
    }
  }
  return std::nullopt;
}

void RemoteProcessGroupPort::refreshPeerList() {
  auto connection = refreshRemoteSiteToSiteInfo();
  if (!connection) {
    logger_->log_warn("No port configured");
    return;
  }

  peers_.clear();

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol;
  sitetosite::SiteToSiteClientConfiguration config(protocol_uuid_, connection->first, connection->second, local_network_interface_, client_type_);
  protocol = initializeProtocol(config);

  if (protocol) {
    if (auto peers = protocol->getPeerList()) {
      peers_ = *peers;
    }
  }

  logger_->log_info("Have {} peers", peers_.size());

  if (!peers_.empty()) {
    peer_index_ = 0;
  }
}

}  // namespace org::apache::nifi::minifi
