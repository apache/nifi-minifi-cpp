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

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include "HTTPTransaction.h"
#include "sitetosite/SiteToSite.h"
#include "sitetosite/SiteToSiteClient.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "sitetosite/Peer.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

/**
 * Site2Site Peer
 */
typedef struct Site2SitePeerStatus {
  std::string host_;
  int port_;
  bool isSecure_;
} Site2SitePeerStatus;

// HttpSiteToSiteClient Class
class HttpSiteToSiteClient : public sitetosite::SiteToSiteClient {
  static constexpr char const* PROTOCOL_VERSION_HEADER = "x-nifi-site-to-site-protocol-version";

 public:
  /*!
   * Create a new http protocol
   */
  explicit HttpSiteToSiteClient(const std::string& /*name*/, const utils::Identifier& /*uuid*/ = {})
      : SiteToSiteClient(),
        current_code(UNRECOGNIZED_RESPONSE_CODE),
        logger_(logging::LoggerFactory<HttpSiteToSiteClient>::getLogger()) {
    peer_state_ = READY;
  }

  /*!
   * Create a new http protocol
   */
  explicit HttpSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer)
      : SiteToSiteClient(),
        current_code(UNRECOGNIZED_RESPONSE_CODE),
        logger_(logging::LoggerFactory<HttpSiteToSiteClient>::getLogger()) {
    peer_ = std::move(peer);
    peer_state_ = READY;
  }
  // Destructor
  virtual ~HttpSiteToSiteClient() = default;

  void setPeer(std::unique_ptr<SiteToSitePeer> peer) override {
    peer_ = std::move(peer);
  }

  bool getPeerList(std::vector<PeerStatus> &peers) override;

  /**
   * Establish the protocol connection. Not needed for the HTTP connection, so we simply
   * return true.
   */
  bool establish() override {
    return true;
  }

  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;

  // Transfer flow files for the process session
  // virtual bool transferFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
  //! Transfer string for the process session
  bool transmitPayload(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session, const std::string &payload,
                               std::map<std::string, std::string> attributes) override;
  // deleteTransaction
  void deleteTransaction(const utils::Identifier& transactionID) override;

 protected:
  /**
   * Closes the transaction
   * @param transactionID transaction id reference.
   */
  void closeTransaction(const utils::Identifier &transactionID);

  int readResponse(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message) override;
  // write respond
  int writeResponse(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message) override;

  /**
   * Bootstrapping is not really required for the HTTP Site To Site so we will set the peer state and return true.
   */
  bool bootstrap() override {
    peer_state_ = READY;
    return true;
  }

  /**
   * Creates a connection for sending, returning an HTTP client that is structured and configured
   * to receive flow files.
   * @param transaction transaction against which we are performing our sends
   * @return HTTP Client shared pointer.
   */
  std::shared_ptr<minifi::utils::HTTPClient> openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction);

  /**
   * Creates a connection for receiving, returning an HTTP client that is structured and configured
   * to receive flow files.
   * @param transaction transaction against which we are performing our reads
   * @return HTTP Client shared pointer.
   */
  std::shared_ptr<minifi::utils::HTTPClient> openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction);

  const std::string getBaseURI() {
    std::string uri = ssl_context_service_ != nullptr ? "https://" : "http://";
    uri.append(peer_->getHostName());
    uri.append(":");
    uri.append(std::to_string(peer_->getPort()));
    uri.append("/nifi-api/");
    return uri;
  }

  void tearDown() override;

  utils::optional<utils::Identifier> parseTransactionId(const std::string &uri);

  std::unique_ptr<utils::HTTPClient> create_http_client(const std::string &uri, const std::string &method = "POST", bool setPropertyHeaders = false) {
    std::unique_ptr<utils::HTTPClient> http_client_ = std::unique_ptr<utils::HTTPClient>(new minifi::utils::HTTPClient(uri, ssl_context_service_));
    http_client_->initialize(method, uri, ssl_context_service_);
    if (setPropertyHeaders) {
      if (_currentVersion >= 5) {
        // batch count, size, and duratin don't appear to be set through the interfaces.
      }
    }
    if (!this->peer_->getInterface().empty()) {
      logger_->log_info("HTTP Site2Site bind local network interface %s", this->peer_->getInterface());
      http_client_->setInterface(this->peer_->getInterface());
    }
    if (!this->peer_->getHTTPProxy().host.empty()) {
      logger_->log_info("HTTP Site2Site setup http proxy host %s", this->peer_->getHTTPProxy().host);
      http_client_->setHTTPProxy(this->peer_->getHTTPProxy());
    }
    http_client_->setReadTimeout(idle_timeout_);
    return http_client_;
  }

 private:
  RespondCode current_code;
  std::shared_ptr<logging::Logger> logger_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  HttpSiteToSiteClient(const HttpSiteToSiteClient &parent);
  HttpSiteToSiteClient &operator=(const HttpSiteToSiteClient &parent);
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
