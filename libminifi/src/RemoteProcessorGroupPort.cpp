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

#include <curl/curl.h>
#include <curl/easy.h>
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
      if (url_.empty()) {
        sitetosite::SiteToSiteClientConfiguration config(stream_factory_, std::make_shared<sitetosite::Peer>(protocol_uuid_, host_, port_, ssl_service != nullptr), this->getInterface(), client_type_);
        config.setHTTPProxy(this->proxy_);
        nextProtocol = sitetosite::createClient(config);
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

  client_type_ = sitetosite::RAW;
  std::string http_enabled_str;
  if (configure_->get(Configure::nifi_remote_input_http, http_enabled_str)) {
    if (utils::StringUtils::StringToBool(http_enabled_str, http_enabled_)) {
      client_type_ = sitetosite::HTTP;
    }
  }
  logger_->log_trace("Finished initialization");
}

void RemoteProcessorGroupPort::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  if (context->getProperty(portUUID.getName(), value)) {
    uuid_parse(value.c_str(), protocol_uuid_);
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

  bool disable = false;
  if (ssl_service && configure_->get(Configure::nifi_security_client_disable_host_verification, value) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, disable)) {
    ssl_service->setDisableHostVerification();
  }
  disable = false;
  if (ssl_service && configure_->get(Configure::nifi_security_client_disable_peer_verification, value) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, disable)) {
    ssl_service->setDisablePeerVerification();
  }

  std::lock_guard<std::mutex> lock(peer_mutex_);
  if (!url_.empty()) {
    refreshPeerList();
    if (peers_.size() > 0)
      peer_index_ = 0;
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
  if (url_.empty()) {
    if (context->getProperty(hostName.getName(), value) && !value.empty()) {
      host_ = value;
    }

    int64_t lvalue;
    if (context->getProperty(port.getName(), value) && !value.empty() && core::Property::StringToInt(value, lvalue)) {
      port_ = static_cast<int>(lvalue);
    }
  }

  if (context->getProperty(portUUID.getName(), value) && !value.empty()) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }

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

void RemoteProcessorGroupPort::refreshRemoteSite2SiteInfo() {
  if (this->host_.empty() || this->port_ == -1 || this->protocol_.empty())
    return;

  std::string fullUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/site-to-site";

  this->site2site_port_ = -1;
  configure_->get(Configure::nifi_rest_api_user_name, this->rest_user_name_);
  configure_->get(Configure::nifi_rest_api_password, this->rest_password_);

  std::string token;
  std::unique_ptr<utils::BaseHTTPClient> client = nullptr;
  if (!rest_user_name_.empty()) {
    std::string loginUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/access/token";

    auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
    if (nullptr == client_ptr) {
      logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
      return;
    }
    client = std::unique_ptr<utils::BaseHTTPClient>(dynamic_cast<utils::BaseHTTPClient*>(client_ptr));
    client->initialize("GET", loginUrl, ssl_service);
    if (ssl_service) {
      if (ssl_service->getDisableHostVerification()) {
        client->setDisableHostVerification();
      }
      if (ssl_service->getDisablePeerVerification()) {
        client->setDisablePeerVerification();
      }
    }

    token = utils::get_token(client.get(), this->rest_user_name_, this->rest_password_);
    logger_->log_debug("Token from NiFi REST Api endpoint %s,  %s", loginUrl, token);
    if (token.empty())
      return;
  }

  auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
  if (nullptr == client_ptr) {
    logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
    return;
  }
  client = std::unique_ptr<utils::BaseHTTPClient>(dynamic_cast<utils::BaseHTTPClient*>(client_ptr));
  client->initialize("GET", fullUrl.c_str(), ssl_service);
  if (ssl_service) {
    if (ssl_service->getDisableHostVerification()) {
      client->setDisableHostVerification();
    }
    if (ssl_service->getDisablePeerVerification()) {
      client->setDisablePeerVerification();
    }
  }
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
      logger_->log_debug("controller config %s", controller);
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
            this->site2site_port_ = port_itr->value.GetInt();
          else
            this->site2site_port_ = port_;

          if (secure_itr != end_itr && secure_itr->value.IsBool())
            this->site2site_secure_ = secure_itr->value.GetBool();
        }
        logger_->log_debug("process group remote site2site port %d, is secure %d", site2site_port_, site2site_secure_);
      }
    } else {
      logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo: received HTTP code %ll from %s", client->getResponseCode(), fullUrl);
    }
  } else {
    logger_->log_error("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed \n");
  }
}

void RemoteProcessorGroupPort::refreshPeerList() {
  refreshRemoteSite2SiteInfo();
  if (site2site_port_ == -1)
    return;

  this->peers_.clear();

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol;
  sitetosite::SiteToSiteClientConfiguration config(stream_factory_, std::make_shared<sitetosite::Peer>(protocol_uuid_, host_,
    site2site_port_, ssl_service != nullptr), this->getInterface(), client_type_);
  config.setSecurityContext(ssl_service);
  config.setHTTPProxy(this->proxy_);
  protocol = sitetosite::createClient(config);

  protocol->getPeerList(peers_);

  logging::LOG_INFO(logger_) << "Have " << peers_.size() << " peers";

  if (peers_.size() > 0)
    peer_index_ = 0;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
