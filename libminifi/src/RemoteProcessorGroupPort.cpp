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
#include "json/json.h"
#include "json/writer.h"

#include "Exception.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessorNode.h"
#include "core/Property.h"
#include "core/Relationship.h"
#include "Site2SitePeer.h"
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

std::unique_ptr<Site2SiteClientProtocol> RemoteProcessorGroupPort::getNextProtocol(bool create = true) {
  std::unique_ptr<Site2SiteClientProtocol> nextProtocol = nullptr;
  if (!available_protocols_.try_dequeue(nextProtocol)) {
    if (create) {
      // create
      if (url_.empty()) {
        nextProtocol = std::unique_ptr<Site2SiteClientProtocol>(new Site2SiteClientProtocol(nullptr));
        nextProtocol->setPortId(protocol_uuid_);
        std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(stream_factory_->createSocket(host_, port_));
        std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(new Site2SitePeer(std::move(str), host_, port_));
        nextProtocol->setPeer(std::move(peer_));
      } else if (site2site_peer_index_ >= 0) {
        nextProtocol = std::unique_ptr<Site2SiteClientProtocol>(new Site2SiteClientProtocol(nullptr));
        minifi::Site2SitePeerStatus peer;
        nextProtocol->setPortId(protocol_uuid_);
        {
          std::lock_guard<std::mutex> lock(site2site_peer_mutex_);
          peer = site2site_peer_status_list_[this->site2site_peer_index_];
          site2site_peer_index_++;
          if (site2site_peer_index_ >= site2site_peer_status_list_.size()) {
            site2site_peer_index_ = 0;
          }
        }
        logger_->log_info("creating new protocol with %s and %d", peer.host_, peer.port_);
        std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(stream_factory_->createSocket(peer.host_, peer.port_));
        std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(new Site2SitePeer(std::move(str), peer.host_, peer.port_));
        nextProtocol->setPeer(std::move(peer_));
      } else {
        logger_->log_info("Refreshing the peer list since there are none configured.");
        refreshPeerList();
      }
    }
  }
  return std::move(nextProtocol);
}

void RemoteProcessorGroupPort::returnProtocol(std::unique_ptr<Site2SiteClientProtocol> return_protocol) {
  int count = site2site_peer_status_list_.size();
  if (max_concurrent_tasks_ > count)
    count = max_concurrent_tasks_;
  if (available_protocols_.size_approx() >= count) {
    // let the memory be freed
    return;
  }
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
  std::lock_guard<std::mutex> lock(site2site_peer_mutex_);
  if (!url_.empty()) {
    refreshPeerList();
    if (site2site_peer_status_list_.size() > 0)
      site2site_peer_index_ = 0;
  }
  // populate the site2site protocol for load balancing between them
  if (site2site_peer_status_list_.size() > 0) {
    int count = site2site_peer_status_list_.size();
    if (max_concurrent_tasks_ > count)
      count = max_concurrent_tasks_;
    for (int i = 0; i < count; i++) {
      std::unique_ptr<Site2SiteClientProtocol> nextProtocol = nullptr;
      nextProtocol = std::unique_ptr<Site2SiteClientProtocol>(new Site2SiteClientProtocol(nullptr));
      nextProtocol->setPortId(protocol_uuid_);
      minifi::Site2SitePeerStatus peer = site2site_peer_status_list_[this->site2site_peer_index_];
      site2site_peer_index_++;
      if (site2site_peer_index_ >= site2site_peer_status_list_.size()) {
        site2site_peer_index_ = 0;
      }
      std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(stream_factory_->createSocket(peer.host_, peer.port_));
      std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(new Site2SitePeer(std::move(str), peer.host_, peer.port_));
      nextProtocol->setPeer(std::move(peer_));
      returnProtocol(std::move(nextProtocol));
    }
  }
}

void RemoteProcessorGroupPort::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
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
  }
}

void RemoteProcessorGroupPort::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  if (!transmitting_) {
    return;
  }

  std::string value;

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

  std::unique_ptr<Site2SiteClientProtocol> protocol_ = nullptr;
  try {
    protocol_ = getNextProtocol();

    if (!protocol_) {
      logger_->log_info("no protocol");
      context->yield();
      return;
    }
    logger_->log_info("got protocol");
    if (!protocol_->bootstrap()) {
      // bootstrap the client protocol if needed
      context->yield();
      std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(context->getProcessorNode()->getProcessor());
      logger_->log_error("Site2Site bootstrap failed yield period %d peer ", processor->getYieldPeriodMsec());

      return;
    }

    if (direction_ == RECEIVE) {
      protocol_->receiveFlowFiles(context, session);
    } else {
      protocol_->transferFlowFiles(context, session);
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

  std::string fullUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/controller";

  this->site2site_port_ = -1;
  configure_->get(Configure::nifi_rest_api_user_name, this->rest_user_name_);
  configure_->get(Configure::nifi_rest_api_password, this->rest_password_);

  std::string token;

  if (!rest_user_name_.empty()) {
    std::string loginUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/access/token";
    utils::HTTPClient client(loginUrl, ssl_service);
    client.setVerbose();
    token = utils::get_token(client, this->rest_user_name_, this->rest_password_);
    logger_->log_debug("Token from NiFi REST Api endpoint %s,  %s", loginUrl, token);
    if (token.empty())
      return;
  }

  utils::HTTPClient client(fullUrl.c_str(), ssl_service);

  client.initialize("GET");

  struct curl_slist *list = NULL;
  if (!token.empty()) {
    std::string header = "Authorization: " + token;
    list = curl_slist_append(list, header.c_str());
    client.setHeaders(list);
  }

  if (client.submit() && client.getResponseCode() == 200) {
    const std::vector<char> &response_body = client.getResponseBody();
    if (!response_body.empty()) {
      std::string controller = std::string(response_body.begin(), response_body.end());
      logger_->log_debug("controller config %s", controller.c_str());
      Json::Value value;
      Json::Reader reader;
      bool parsingSuccessful = reader.parse(controller, value);
      if (parsingSuccessful && !value.empty()) {
        Json::Value controllerValue = value["controller"];
        if (!controllerValue.empty()) {
          Json::Value port = controllerValue["remoteSiteListeningPort"];
          if (!port.empty())
            this->site2site_port_ = port.asInt();
          Json::Value secure = controllerValue["siteToSiteSecure"];
          if (!secure.empty())
            this->site2site_secure_ = secure.asBool();
        }
        logger_->log_info("process group remote site2site port %d, is secure %d", site2site_port_, site2site_secure_);
      }
    } else {
      logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo: received HTTP code %d from %s", client.getResponseCode(), fullUrl);
    }
  } else {
    logger_->log_error("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed \n");
  }
}

void RemoteProcessorGroupPort::refreshPeerList() {
  refreshRemoteSite2SiteInfo();
  if (site2site_port_ == -1)
    return;

  this->site2site_peer_status_list_.clear();

  std::unique_ptr<Site2SiteClientProtocol> protocol;
  protocol = std::unique_ptr<Site2SiteClientProtocol>(new Site2SiteClientProtocol(nullptr));
  protocol->setPortId(protocol_uuid_);
  std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(stream_factory_->createSocket(host_, site2site_port_));
  std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(new Site2SitePeer(std::move(str), host_, site2site_port_));
  protocol->setPeer(std::move(peer_));
  protocol->getPeerList(site2site_peer_status_list_);

  if (site2site_peer_status_list_.size() > 0)
    site2site_peer_index_ = 0;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
