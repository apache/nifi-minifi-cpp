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

#include <uuid/uuid.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <deque>
#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <type_traits>
#include <utility>

#include "sitetosite/Peer.h"
#include "Exception.h"
#include "sitetosite/SiteToSiteFactory.h"

#include "rapidjson/document.h"

#include "Exception.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessorNode.h"
#include "core/Property.h"
#include "core/Relationship.h"
#include "utils/HTTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const char *RemoteProcessorGroupPort::RPG_SSL_CONTEXT_SERVICE_NAME = "RemoteProcessorGroupPortSSLContextService";

const char *RemoteProcessorGroupPort::ProcessorName("RemoteProcessorGroupPort");
core::Property RemoteProcessorGroupPort::hostName("Host Name", "Remote Host Name.", "");
core::Property RemoteProcessorGroupPort::SSLContext("SSL Context Service", "The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.", "");
core::Property RemoteProcessorGroupPort::port("Port", "Remote Port", "");
core::Property RemoteProcessorGroupPort::portUUID("Port UUID", "Specifies remote NiFi Port UUID.", "");
core::Relationship RemoteProcessorGroupPort::relation;

std::unique_ptr<sitetosite::SiteToSiteClient> RemoteProcessorGroupPort::getNextProtocol(bool create = true) {
  std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
  if (!available_protocols_.try_dequeue(nextProtocol)) {
    if (create) {
      // create
      if (bypass_rest_api_) {
        if (nifi_instances_.size() > 0) {
          auto rpg = nifi_instances_.front();
          sitetosite::SiteToSiteClientConfiguration config(stream_factory_, std::make_shared<sitetosite::Peer>(protocol_uuid_, rpg.host_, rpg.port_, ssl_service != nullptr), this->getInterface(),
                                                           client_type_);
          config.setHTTPProxy(this->proxy_);
          nextProtocol = sitetosite::createClient(config);
        }
      } else if (peer_index_ >= 0) {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        logger_->log_debug("Creating client from peer %ll", peer_index_.load());
        sitetosite::SiteToSiteClientConfiguration config(stream_factory_, peers_[this->peer_index_].getPeer(), local_network_interface_, client_type_);
        config.setSecurityContext(ssl_service);
        peer_index_++;
        if (peer_index_ >= static_cast<int>(peers_.size())) {
          peer_index_ = 0;
        }
        config.setHTTPProxy(this->proxy_);
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
    logger_->log_debug("not enqueueing protocol %s", getUUIDStr());
    // let the memory be freed
    return;
  }
  logger_->log_debug("enqueueing protocol %s, have a total of %lu", getUUIDStr(), available_protocols_.size_approx());
  available_protocols_.enqueue(std::move(return_protocol));
}

void RemoteProcessorGroupPort::initialize() {
// Set the supported properties
  std::set<core::Property> properties;
  properties.insert(hostName);
  properties.insert(port);
  properties.insert(SSLContext);
  properties.insert(portUUID);
  setSupportedProperties(properties);
// Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);

  logger_->log_trace("Finished initialization");
}

void RemoteProcessorGroupPort::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  if (context->getProperty(portUUID.getName(), value) && !value.empty()) {
    protocol_uuid_ = value;
  }

  std::string http_enabled_str;
  if (configure_->get(Configure::nifi_remote_input_http, http_enabled_str)) {
    if (utils::StringUtils::StringToBool(http_enabled_str, http_enabled_)) {
      if (client_type_ == sitetosite::CLIENT_TYPE::RAW) {
        logger_->log_trace("Remote Input HTTP Enabled, but raw has been suggested for %s", protocol_uuid_.to_string());
      }
    }
  }

  std::string context_name;
  if (!context->getProperty(SSLContext.getName(), context_name) || IsNullOrEmpty(context_name)) {
    context_name = RPG_SSL_CONTEXT_SERVICE_NAME;
  }
  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(context_name);
  if (nullptr != service) {
    ssl_service = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
  } else {
    std::string secureStr;
    bool is_secure = false;
    if (configure_->get(Configure::nifi_remote_input_secure, secureStr) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(secureStr, is_secure)) {
      ssl_service = std::make_shared<minifi::controllers::SSLContextService>(RPG_SSL_CONTEXT_SERVICE_NAME, configure_);
      ssl_service->onEnable();
    }
  }

  std::lock_guard<std::mutex> lock(peer_mutex_);
  if (!nifi_instances_.empty()) {
    refreshPeerList();
    if (peers_.size() > 0)
      peer_index_ = 0;
  }
  /**
   * If at this point we have no peers and HTTP support is disabled this means
   * we must rely on the configured host/port
   */
  if (peers_.empty() && is_http_disabled()) {
    std::string host, portStr;
    int configured_port = -1;
    // place hostname/port into the log message if we have it
    context->getProperty(hostName.getName(), host);
    context->getProperty(port.getName(), portStr);
    if (!host.empty() && !portStr.empty() && !portStr.empty() && core::Property::StringToInt(portStr, configured_port)) {
      nifi_instances_.push_back({ host, configured_port, "" });
      bypass_rest_api_ = true;
    } else {
      // we cannot proceed, so log error and throw an exception
      logger_->log_error("%s/%s/%d -- configuration values after eval of configuration options", host, portStr, configured_port);
      throw(Exception(SITE2SITE_EXCEPTION, "HTTPClient not resolvable. No peers configured or any port specific hostname and port -- cannot schedule"));
    }
  }
  // populate the site2site protocol for load balancing between them
  if (peers_.size() > 0) {
    auto count = peers_.size();
    if (max_concurrent_tasks_ > count)
      count = max_concurrent_tasks_;
    for (uint32_t i = 0; i < count; i++) {
      std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
      sitetosite::SiteToSiteClientConfiguration config(stream_factory_, peers_[this->peer_index_].getPeer(), this->getInterface(), client_type_);
      config.setSecurityContext(ssl_service);
      peer_index_++;
      if (peer_index_ >= static_cast<int>(peers_.size())) {
        peer_index_ = 0;
      }
      logger_->log_trace("Creating client");
      config.setHTTPProxy(this->proxy_);
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

void RemoteProcessorGroupPort::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("On trigger %s", getUUIDStr());
  if (!transmitting_) {
    return;
  }

  RPGLatch count;

  std::string value;

  logger_->log_trace("On trigger %s", getUUIDStr());

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol_ = nullptr;
  try {
    logger_->log_trace("get protocol in on trigger");
    protocol_ = getNextProtocol();

    if (!protocol_) {
      logger_->log_info("no protocol, yielding");
      context->yield();
      return;
    }

    if (!protocol_->transfer(direction_, context, session)) {
      logger_->log_warn("protocol transmission failed, yielding");
      context->yield();
    }

    returnProtocol(std::move(protocol_));
    return;
  } catch (const minifi::Exception &ex2) {
    context->yield();
    session->rollback();
  } catch (...) {
    context->yield();
    session->rollback();
  }
}

std::pair<std::string, int> RemoteProcessorGroupPort::refreshRemoteSite2SiteInfo() {
  if (nifi_instances_.empty())
    return std::make_pair("", -1);

  for (auto nifi : nifi_instances_) {
    std::string host = nifi.host_;
    std::string protocol = nifi.protocol_;
    int port = nifi.port_;
    std::stringstream fullUrl;
    fullUrl << protocol << host;
    // don't append port if it is 0 ( undefined )
    if (port > 0) {
      fullUrl << ":" << std::to_string(port);
    }
    fullUrl << "/nifi-api/site-to-site";

    configure_->get(Configure::nifi_rest_api_user_name, this->rest_user_name_);
    configure_->get(Configure::nifi_rest_api_password, this->rest_password_);

    std::string token;
    std::unique_ptr<utils::BaseHTTPClient> client = nullptr;
    if (!rest_user_name_.empty()) {
      std::stringstream loginUrl;
      fullUrl << protocol << host;
      // don't append port if it is 0 ( undefined )
      if (port > 0) {
        fullUrl << ":" << std::to_string(port);
      }
      fullUrl << "/nifi-api/access/token";

      auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
      if (nullptr == client_ptr) {
        logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
        return std::make_pair("", -1);
      }
      client = std::unique_ptr<utils::BaseHTTPClient>(dynamic_cast<utils::BaseHTTPClient*>(client_ptr));
      client->initialize("GET", loginUrl.str(), ssl_service);
      // use a connection timeout. if this times out we will simply attempt re-connection
      // so no need for configuration parameter that isn't already defined in Processor
      client->setConnectionTimeout(10);

      token = utils::get_token(client.get(), this->rest_user_name_, this->rest_password_);
      logger_->log_debug("Token from NiFi REST Api endpoint %s,  %s", loginUrl.str(), token);
      if (token.empty())
        return std::make_pair("", -1);
    }

    auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
    if (nullptr == client_ptr) {
      logger_->log_error("Could not locate HTTPClient. You do not have cURL support, defaulting to base configuration!");
      return std::make_pair("", -1);
    }
    int siteTosite_port_ = -1;
    client = std::unique_ptr<utils::BaseHTTPClient>(dynamic_cast<utils::BaseHTTPClient*>(client_ptr));
    client->initialize("GET", fullUrl.str().c_str(), ssl_service);
    // use a connection timeout. if this times out we will simply attempt re-connection
    // so no need for configuration parameter that isn't already defined in Processor
    client->setConnectionTimeout(10);
    if (!proxy_.host.empty()) {
      client->setHTTPProxy(proxy_);
    }
    if (!token.empty()) {
      std::string header = "Authorization: " + token;
      client->appendHeader(header);
    }

    if (client->submit() && client->getResponseCode() == 200) {
      const std::vector<char> &response_body = client->getResponseBody();
      if (!response_body.empty()) {
        std::string controller = std::string(response_body.begin(), response_body.end());
        logger_->log_trace("controller config %s", controller);
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
              siteTosite_port_ = port;

            if (secure_itr != end_itr && secure_itr->value.IsBool())
              this->site2site_secure_ = secure_itr->value.GetBool();
          }
          logger_->log_debug("process group remote site2site port %d, is secure %d", siteTosite_port_, site2site_secure_);
          return std::make_pair(host, siteTosite_port_);
        }
      } else {
        logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo: received HTTP code %ll from %s", client->getResponseCode(), fullUrl.str());
      }
    } else {
      logger_->log_error("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed , response code %d\n", client->getResponseCode());
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
  sitetosite::SiteToSiteClientConfiguration config(stream_factory_, std::make_shared<sitetosite::Peer>(protocol_uuid_, connection.first, connection.second, ssl_service != nullptr),
                                                   this->getInterface(), client_type_);
  config.setSecurityContext(ssl_service);
  config.setHTTPProxy(this->proxy_);
  protocol = sitetosite::createClient(config);

  if (protocol)
    protocol->getPeerList(peers_);

  logging::LOG_INFO(logger_) << "Have " << peers_.size() << " peers";

  if (peers_.size() > 0)
    peer_index_ = 0;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
