/**
 * @file RemoteProcessorGroupPort.cpp
 * RemoteProcessorGroupPort class implementation
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

#include "RemoteProcessorGroupPort.h"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cinttypes>

#include "sitetosite/Peer.h"
#include "Exception.h"
#include "sitetosite/SiteToSiteFactory.h"

#include "rapidjson/document.h"

#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessorNode.h"
#include "http/BaseHTTPClient.h"
#include "controllers/SSLContextService.h"
#include "utils/net/DNS.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

const char *RemoteProcessorGroupPort::RPG_SSL_CONTEXT_SERVICE_NAME = "RemoteProcessorGroupPortSSLContextService";

std::unique_ptr<sitetosite::SiteToSiteClient> RemoteProcessorGroupPort::getNextProtocol(bool create = true) {
  std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
  if (!available_protocols_.try_dequeue(nextProtocol)) {
    if (create) {
      // create
      if (bypass_rest_api_) {
        if (!nifi_instances_.empty()) {
          auto rpg = nifi_instances_.front();
          auto host = rpg.host_;
#ifdef WIN32
          if ("localhost" == host) {
            host = org::apache::nifi::minifi::utils::net::getMyHostName();
          }
#endif
          sitetosite::SiteToSiteClientConfiguration config(std::make_shared<sitetosite::Peer>(protocol_uuid_, host, rpg.port_, ssl_service != nullptr), this->getInterface(),
                                                           client_type_);
          config.setHTTPProxy(this->proxy_);
          config.setIdleTimeout(idle_timeout_);
          nextProtocol = sitetosite::createClient(config);
        }
      } else if (peer_index_ >= 0) {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        logger_->log_debug("Creating client from peer {}", peer_index_.load());
        sitetosite::SiteToSiteClientConfiguration config(peers_[this->peer_index_].getPeer(), local_network_interface_, client_type_);
        config.setSecurityContext(ssl_service);
        peer_index_++;
        if (peer_index_ >= static_cast<int>(peers_.size())) {
          peer_index_ = 0;
        }
        config.setHTTPProxy(this->proxy_);
        config.setIdleTimeout(idle_timeout_);
        nextProtocol = sitetosite::createClient(config);
      } else {
        logger_->log_debug("Refreshing the peer list since there are none configured.");
        refreshPeerList();
      }
    }
  }
  logger_->log_debug("Obtained protocol from available_protocols_");
  return nextProtocol;
}

void RemoteProcessorGroupPort::returnProtocol(std::unique_ptr<sitetosite::SiteToSiteClient> return_protocol) {
  auto count = peers_.size();
  if (max_concurrent_tasks_ > count)
    count = max_concurrent_tasks_;
  if (available_protocols_.size_approx() >= count) {
    logger_->log_debug("not enqueueing protocol {}", getUUIDStr());
    // let the memory be freed
    return;
  }
  logger_->log_debug("enqueueing protocol {}, have a total of {}", getUUIDStr(), available_protocols_.size_approx());
  available_protocols_.enqueue(std::move(return_protocol));
}

void RemoteProcessorGroupPort::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);

  logger_->log_trace("Finished initialization");
}

void RemoteProcessorGroupPort::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  std::string value;
  if (context.getProperty(portUUID, value) && !value.empty()) {
    protocol_uuid_ = value;
  }

  std::string context_name;
  if (!context.getProperty(SSLContext, context_name) || IsNullOrEmpty(context_name)) {
    context_name = RPG_SSL_CONTEXT_SERVICE_NAME;
  }
  std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(context_name);
  if (nullptr != service) {
    ssl_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
  } else {
    std::string secureStr;
    if (configure_->get(Configure::nifi_remote_input_secure, secureStr) && utils::string::toBool(secureStr).value_or(false)) {
      ssl_service = std::make_shared<minifi::controllers::SSLContextServiceImpl>(RPG_SSL_CONTEXT_SERVICE_NAME, configure_);
      ssl_service->onEnable();
    }
  }
  {
    if (auto idle_timeout = context.getProperty<core::TimePeriodValue>(idleTimeout)) {
      idle_timeout_ = idle_timeout->getMilliseconds();
    } else {
      static_assert(idleTimeout.default_value);
      logger_->log_debug("{} attribute is invalid, so default value of {} will be used", idleTimeout.name, *idleTimeout.default_value);
      idle_timeout_ = core::TimePeriodValue(std::string(*idleTimeout.default_value)).getMilliseconds();
    }
  }

  std::lock_guard<std::mutex> lock(peer_mutex_);
  if (!nifi_instances_.empty()) {
    refreshPeerList();
    if (!peers_.empty())
      peer_index_ = 0;
  }
  // populate the site2site protocol for load balancing between them
  if (!peers_.empty()) {
    auto count = peers_.size();
    if (max_concurrent_tasks_ > count)
      count = max_concurrent_tasks_;
    for (uint32_t i = 0; i < count; i++) {
      std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
      sitetosite::SiteToSiteClientConfiguration config(peers_[this->peer_index_].getPeer(), this->getInterface(), client_type_);
      config.setSecurityContext(ssl_service);
      peer_index_++;
      if (peer_index_ >= static_cast<int>(peers_.size())) {
        peer_index_ = 0;
      }
      logger_->log_trace("Creating client");
      config.setHTTPProxy(this->proxy_);
      config.setIdleTimeout(idle_timeout_);
      nextProtocol = sitetosite::createClient(config);
      logger_->log_trace("Created client, moving into available protocols");
      returnProtocol(std::move(nextProtocol));
    }
  } else {
    // we don't have any peers
    logger_->log_error("No peers selected during scheduling");
  }
}

void RemoteProcessorGroupPort::notifyStop() {
  transmitting_ = false;
  RPGLatch count(false);  // we're just a monitor
  // we use the latch
  while (count.getCount() > 0) {
  }
  std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
  while (available_protocols_.try_dequeue(nextProtocol)) {
    // clear all protocols now
  }
}

void RemoteProcessorGroupPort::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("On trigger {}", getUUIDStr());
  if (!transmitting_) {
    return;
  }

  RPGLatch count;

  std::string value;

  logger_->log_trace("On trigger {}", getUUIDStr());

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol_ = nullptr;
  try {
    logger_->log_trace("get protocol in on trigger");
    protocol_ = getNextProtocol();

    if (!protocol_) {
      logger_->log_info("no protocol, yielding");
      context.yield();
      return;
    }

    if (!protocol_->transfer(direction_, context, session)) {
      logger_->log_warn("protocol transmission failed, yielding");
      context.yield();
    }

    returnProtocol(std::move(protocol_));
    return;
  } catch (const minifi::Exception &) {
    context.yield();
    session.rollback();
  } catch (...) {
    context.yield();
    session.rollback();
  }
}

std::pair<std::string, int> RemoteProcessorGroupPort::refreshRemoteSite2SiteInfo() {
  if (nifi_instances_.empty())
    return std::make_pair("", -1);

  for (const auto& nifi : nifi_instances_) {
    std::string host = nifi.host_;
#ifdef WIN32
    if ("localhost" == host) {
      host = org::apache::nifi::minifi::utils::net::getMyHostName();
    }
#endif
    std::string protocol = nifi.protocol_;
    int nifi_port = nifi.port_;
    std::stringstream fullUrl;
    fullUrl << protocol << host;
    // don't append port if it is 0 ( undefined )
    if (nifi_port > 0) {
      fullUrl << ":" << std::to_string(nifi_port);
    }
    fullUrl << "/nifi-api/site-to-site";

    configure_->get(Configure::nifi_rest_api_user_name, this->rest_user_name_);
    configure_->get(Configure::nifi_rest_api_password, this->rest_password_);

    std::string token;
    std::unique_ptr<http::BaseHTTPClient> client;
    if (!rest_user_name_.empty()) {
      std::stringstream loginUrl;
      loginUrl << protocol << host;
      // don't append port if it is 0 ( undefined )
      if (nifi_port > 0) {
        loginUrl << ":" << std::to_string(nifi_port);
      }
      loginUrl << "/nifi-api/access/token";

      auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
      if (nullptr == client_ptr) {
        logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
        return std::make_pair("", -1);
      }
      client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
      client->initialize(http::HttpRequestMethod::GET, loginUrl.str(), ssl_service);
      // use a connection timeout. if this times out we will simply attempt re-connection
      // so no need for configuration parameter that isn't already defined in Processor
      client->setConnectionTimeout(10s);
      client->setReadTimeout(idle_timeout_);

      token = http::get_token(client.get(), this->rest_user_name_, this->rest_password_);
      logger_->log_debug("Token from NiFi REST Api endpoint {},  {}", loginUrl.str(), token);
      if (token.empty())
        return std::make_pair("", -1);
    }

    auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
    if (nullptr == client_ptr) {
      logger_->log_error("Could not locate HTTPClient. You do not have cURL support, defaulting to base configuration!");
      return std::make_pair("", -1);
    }
    int siteTosite_port_ = -1;
    client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
    client->initialize(http::HttpRequestMethod::GET, fullUrl.str(), ssl_service);
    // use a connection timeout. if this times out we will simply attempt re-connection
    // so no need for configuration parameter that isn't already defined in Processor
    client->setConnectionTimeout(10s);
    client->setReadTimeout(idle_timeout_);
    if (!proxy_.host.empty()) {
      client->setHTTPProxy(proxy_);
    }
    if (!token.empty())
      client->setRequestHeader("Authorization", token);

    client->setVerbose(false);

    if (client->submit() && client->getResponseCode() == 200) {
      const std::vector<char> &response_body = client->getResponseBody();
      if (!response_body.empty()) {
        std::string controller = std::string(response_body.begin(), response_body.end());
        logger_->log_trace("controller config {}", controller);
        rapidjson::Document doc;
        rapidjson::ParseResult ok = doc.Parse(controller.c_str());

        if (ok && doc.IsObject() && !doc.ObjectEmpty()) {
          rapidjson::Value::MemberIterator itr = doc.FindMember("controller");

          if (itr != doc.MemberEnd() && itr->value.IsObject()) {
            rapidjson::Value controllerValue = itr->value.GetObject();
            rapidjson::Value::ConstMemberIterator end_itr = controllerValue.MemberEnd();
            rapidjson::Value::ConstMemberIterator port_itr = controllerValue.FindMember("remoteSiteListeningPort");
            rapidjson::Value::ConstMemberIterator secure_itr = controllerValue.FindMember("siteToSiteSecure");

            if (client_type_ == sitetosite::CLIENT_TYPE::RAW && port_itr != end_itr && port_itr->value.IsNumber())
              siteTosite_port_ = port_itr->value.GetInt();
            else
              siteTosite_port_ = nifi_port;

            if (secure_itr != end_itr && secure_itr->value.IsBool())
              this->site2site_secure_ = secure_itr->value.GetBool();
          }
          logger_->log_debug("process group remote site2site port {}, is secure {}", siteTosite_port_, site2site_secure_);
          return std::make_pair(host, siteTosite_port_);
        }
      } else {
        logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo: received HTTP code {} from {}", client->getResponseCode(), fullUrl.str());
      }
    } else {
      logger_->log_error("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed , response code {}\n", client->getResponseCode());
    }
  }
  return std::make_pair("", -1);
}

void RemoteProcessorGroupPort::refreshPeerList() {
  auto connection = refreshRemoteSite2SiteInfo();
  if (connection.second == -1) {
    logger_->log_debug("No port configured");
    return;
  }

  this->peers_.clear();

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol;
  sitetosite::SiteToSiteClientConfiguration config(std::make_shared<sitetosite::Peer>(protocol_uuid_, connection.first, connection.second, ssl_service != nullptr),
                                                   this->getInterface(), client_type_);
  config.setSecurityContext(ssl_service);
  config.setHTTPProxy(this->proxy_);
  config.setIdleTimeout(idle_timeout_);
  protocol = sitetosite::createClient(config);

  if (protocol)
    protocol->getPeerList(peers_);

  logger_->log_info("Have {} peers", peers_.size());

  if (!peers_.empty())
    peer_index_ = 0;
}

}  // namespace org::apache::nifi::minifi
