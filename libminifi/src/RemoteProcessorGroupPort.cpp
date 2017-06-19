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

#include "../include/RemoteProcessorGroupPort.h"

#include <curl/curl.h>
#include <curl/curlbuild.h>
#include <curl/easy.h>
#include <uuid/uuid.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include "json/json.h"
#include "json/writer.h"

#include "../include/core/logging/Logger.h"
#include "../include/core/ProcessContext.h"
#include "../include/core/ProcessorNode.h"
#include "../include/core/Property.h"
#include "../include/core/Relationship.h"
#include "../include/Site2SitePeer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const char *RemoteProcessorGroupPort::ProcessorName("RemoteProcessorGroupPort");
core::Property RemoteProcessorGroupPort::hostName("Host Name", "Remote Host Name.", "");
core::Property RemoteProcessorGroupPort::port("Port", "Remote Port", "");
core::Property RemoteProcessorGroupPort::portUUID("Port UUID", "Specifies remote NiFi Port UUID.", "");
core::Relationship RemoteProcessorGroupPort::relation;

std::unique_ptr<Site2SiteClientProtocol> RemoteProcessorGroupPort::getNextProtocol(
bool create = true) {
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
        nextProtocol->setPortId(protocol_uuid_);
        site2site_peer_mutex_.lock();
        minifi::Site2SitePeerStatus peer = site2site_peer_status_list_[this->site2site_peer_index_];
        site2site_peer_index_++;
        if (site2site_peer_index_ >= site2site_peer_status_list_.size()) {
          site2site_peer_index_ = 0;
        }
        site2site_peer_mutex_.unlock();
        std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(stream_factory_->createSocket(peer.host_, peer.port_));
        std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(new Site2SitePeer(std::move(str), peer.host_, peer.port_));
        nextProtocol->setPeer(std::move(peer_));
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
  properties.insert(portUUID);
  setSupportedProperties(properties);
// Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  site2site_peer_mutex_.lock();
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
      nextProtocol = std::unique_ptr < Site2SiteClientProtocol > (new Site2SiteClientProtocol(nullptr));
      nextProtocol->setPortId(protocol_uuid_);
      minifi::Site2SitePeerStatus peer = site2site_peer_status_list_[this->site2site_peer_index_];
      site2site_peer_index_++;
      if (site2site_peer_index_ >= site2site_peer_status_list_.size()) {
        site2site_peer_index_ = 0;
      }
      std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = std::unique_ptr < org::apache::nifi::minifi::io::DataStream > (stream_factory_->createSocket(peer.host_, peer.port_));
      std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr < Site2SitePeer > (new Site2SitePeer(std::move(str), peer.host_, peer.port_));
      nextProtocol->setPeer(std::move(peer_));
      returnProtocol(std::move(nextProtocol));
    }
  }
  site2site_peer_mutex_.unlock();
}

void RemoteProcessorGroupPort::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;

  int64_t lvalue;

  if (context->getProperty(portUUID.getName(), value)) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }
}

void RemoteProcessorGroupPort::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  if (!transmitting_)
    return;

  std::string value;

  int64_t lvalue;

  if (context->getProperty(hostName.getName(), value) && !value.empty()) {
    host_ = value;
  }

  if (context->getProperty(port.getName(), value) && !value.empty() && core::Property::StringToInt(value, lvalue)) {
    port_ = static_cast<int> (lvalue);
  }

  if (context->getProperty(portUUID.getName(), value) && !value.empty()) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }

  std::unique_ptr<Site2SiteClientProtocol> protocol_ = getNextProtocol();

  if (!protocol_) {
    context->yield();
    return;
  }

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(context->getProcessorNode().getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ", processor->getYieldPeriodMsec());
    returnProtocol(std::move(protocol_));
    return;
  }

  if (direction_ == RECEIVE) {
    protocol_->receiveFlowFiles(context, session);
  } else {
    protocol_->transferFlowFiles(context, session);
  }

  returnProtocol(std::move(protocol_));

  return;
}

void RemoteProcessorGroupPort::refreshRemoteSite2SiteInfo() {
  if (this->host_.empty() || this->port_ == -1 || this->protocol_.empty())
      return;

  std::string fullUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/controller/";

  this->site2site_port_ = -1;
  configure_->get(Configure::nifi_rest_api_user_name, this->rest_user_name_);
  configure_->get(Configure::nifi_rest_api_password, this->rest_password_);

  std::string token;

  if (!rest_user_name_.empty()) {
    utils::HTTPRequestResponse content;
    std::string loginUrl = this->protocol_ + this->host_ + ":" + std::to_string(this->port_) + "/nifi-api/access/token/";
    CURL *login_session = curl_easy_init();
    if (loginUrl.find("https") != std::string::npos) {
       this->securityConfig_.configureSecureConnection(login_session);
     }
    curl_easy_setopt(login_session, CURLOPT_URL, loginUrl.c_str());
    struct curl_slist *list = NULL;
    list = curl_slist_append(list, "Content-Type: application/x-www-form-urlencoded");
    list = curl_slist_append(list, "Accept: text/plain");
    curl_easy_setopt(login_session, CURLOPT_HTTPHEADER, list);
    std::string payload = "username=" + this->rest_user_name_ + "&" + "password=" + this->rest_password_;
    char *output = curl_easy_escape(login_session, payload.c_str(), payload.length());
    curl_easy_setopt(login_session, CURLOPT_WRITEFUNCTION,
        &utils::HTTPRequestResponse::recieve_write);
    curl_easy_setopt(login_session, CURLOPT_WRITEDATA,
        static_cast<void*>(&content));
    curl_easy_setopt(login_session, CURLOPT_POSTFIELDSIZE, strlen(output));
    curl_easy_setopt(login_session, CURLOPT_POSTFIELDS, output);
    curl_easy_setopt(login_session, CURLOPT_POST, 1);
    CURLcode res = curl_easy_perform(login_session);
    curl_slist_free_all(list);
    curl_free(output);
    if (res == CURLE_OK) {
      std::string response_body(content.data.begin(), content.data.end());
      int64_t http_code = 0;
      curl_easy_getinfo(login_session, CURLINFO_RESPONSE_CODE, &http_code);
      char *content_type;
      /* ask for the content-type */
      curl_easy_getinfo(login_session, CURLINFO_CONTENT_TYPE, &content_type);

      bool isSuccess = ((int32_t) (http_code / 100)) == 2
          && res != CURLE_ABORTED_BY_CALLBACK;
      bool body_empty = IsNullOrEmpty(content.data);

      if (isSuccess && !body_empty) {
        logger_->log_debug("Token from NiFi REST Api endpoint %s", response_body);
        token = "Bearer " + response_body;
      } else {
        logger_->log_error("Cannot get token for ProcessGroup::refreshRemoteSite2SiteInfo");
      }
    } else {
      logger_->log_error(
          "ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed %s for login session\n",
          curl_easy_strerror(res));
    }
    curl_easy_cleanup(login_session);
    if (token.empty())
        return;
  }

  CURL *http_session = curl_easy_init();

  if (fullUrl.find("https") != std::string::npos) {
    this->securityConfig_.configureSecureConnection(http_session);
  }

  struct curl_slist *list = NULL;
  if (!token.empty()) {
    std::string header = "Authorization: " + token;
    list = curl_slist_append(list, header.c_str());
    curl_easy_setopt(http_session, CURLOPT_HTTPHEADER, list);
  }

  curl_easy_setopt(http_session, CURLOPT_URL, fullUrl.c_str());

  utils::HTTPRequestResponse content;
  curl_easy_setopt(http_session, CURLOPT_WRITEFUNCTION,
      &utils::HTTPRequestResponse::recieve_write);

  curl_easy_setopt(http_session, CURLOPT_WRITEDATA,
      static_cast<void*>(&content));

  CURLcode res = curl_easy_perform(http_session);
  if (list)
    curl_slist_free_all(list);

  if (res == CURLE_OK) {
    std::string response_body(content.data.begin(), content.data.end());
    int64_t http_code = 0;
    curl_easy_getinfo(http_session, CURLINFO_RESPONSE_CODE, &http_code);
    char *content_type;
    /* ask for the content-type */
    curl_easy_getinfo(http_session, CURLINFO_CONTENT_TYPE, &content_type);

    bool isSuccess = ((int32_t) (http_code / 100)) == 2
        && res != CURLE_ABORTED_BY_CALLBACK;
    bool body_empty = IsNullOrEmpty(content.data);

    if (isSuccess && !body_empty) {
      std::string controller = std::move(response_body);
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
      logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo");
    }
  } else {
    logger_->log_error(
        "ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed %s\n",
        curl_easy_strerror(res));
  }
  curl_easy_cleanup(http_session);
}

void RemoteProcessorGroupPort::refreshPeerList() {
  refreshRemoteSite2SiteInfo();
  if (site2site_port_ == -1)
    return;

  this->site2site_peer_status_list_.clear();

  std::unique_ptr < Site2SiteClientProtocol> protocol;
  protocol = std::unique_ptr < Site2SiteClientProtocol
      > (new Site2SiteClientProtocol(nullptr));
  protocol->setPortId(protocol_uuid_);
  std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
      std::unique_ptr < org::apache::nifi::minifi::io::DataStream
          > (stream_factory_->createSocket(host_, site2site_port_));
  std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr < Site2SitePeer
      > (new Site2SitePeer(std::move(str), host_, site2site_port_));
  protocol->setPeer(std::move(peer_));
  protocol->getPeerList(site2site_peer_status_list_);
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
