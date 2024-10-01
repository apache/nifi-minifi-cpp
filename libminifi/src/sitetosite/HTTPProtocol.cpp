/**
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
#include "sitetosite/HTTPProtocol.h"

#include <chrono>
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <vector>

#include "sitetosite/PeersEntity.h"
#include "io/CRCStream.h"
#include "sitetosite/Peer.h"
#include "io/validation.h"
#include "core/Resource.h"

#undef DELETE  // macro on windows

namespace org::apache::nifi::minifi::sitetosite {

std::shared_ptr<utils::IdGenerator> HttpSiteToSiteClient::id_generator_ = utils::IdGenerator::getIdGenerator();

std::optional<utils::Identifier> HttpSiteToSiteClient::parseTransactionId(const std::string &uri) {
  const size_t last_slash_pos = uri.find_last_of('/');
  size_t id_start_pos = 0;
  if (last_slash_pos != std::string::npos) {
    id_start_pos = last_slash_pos + 1;
  }
  return utils::Identifier::parse(uri.substr(id_start_pos));
}

std::shared_ptr<sitetosite::Transaction> HttpSiteToSiteClient::createTransaction(sitetosite::TransferDirection direction) {
  std::string dir_str = direction == sitetosite::SEND ? "input-ports" : "output-ports";
  std::stringstream uri;
  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions";
  auto client = create_http_client(uri.str(), http::HttpRequestMethod::POST);
  client->setRequestHeader(PROTOCOL_VERSION_HEADER, "1");
  client->setConnectionTimeout(std::chrono::milliseconds(5000));
  client->setContentType("application/json");
  client->setRequestHeader("Accept", "application/json");
  client->setRequestHeader("Transfer-Encoding", "chunked");
  client->setPostFields("");
  client->submit();
  auto http_stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
  if (peer_->getStream() != nullptr)
    logger_->log_debug("Closing {}", http_stream->getClientRef()->getURL());
  if (client->getResponseCode() == 201) {
    // parse the headers
    auto intent_name = client->getHeaderValue("x-location-uri-intent");
    if (utils::string::equalsIgnoreCase(intent_name, "transaction-url")) {
      auto url = client->getHeaderValue("Location");

      if (IsNullOrEmpty(url)) {
        logger_->log_debug("Location is empty");
      } else {
        org::apache::nifi::minifi::io::CRCStream<sitetosite::SiteToSitePeer> crcstream(gsl::make_not_null(peer_.get()));
        auto transaction = std::make_shared<HttpTransaction>(direction, std::move(crcstream));
        transaction->initialize(this, url);
        auto transactionId = parseTransactionId(url);
        if (!transactionId)
          return nullptr;
        transaction->setTransactionId(transactionId.value());
        std::shared_ptr<minifi::http::HTTPClient> transaction_client;
        if (transaction->getDirection() == sitetosite::SEND) {
          transaction_client = openConnectionForSending(transaction);
        } else {
          transaction_client = openConnectionForReceive(transaction);
          transaction->setDataAvailable(true);
          // 201 tells us that data is available. 200 would mean that nothing is available.
        }

        transaction_client->setRequestHeader(PROTOCOL_VERSION_HEADER, "1");
        peer_->setStream(std::unique_ptr<io::BaseStream>(new http::HttpStream(transaction_client)));
        logger_->log_debug("Created transaction id -{}-", transaction->getUUID().to_string());
        known_transactions_[transaction->getUUID()] = transaction;
        return transaction;
      }
    } else {
      logger_->log_debug("Could not create transaction, intent is {}", intent_name);
    }
  } else {
    peer_->setStream(nullptr);
    logger_->log_debug("Could not create transaction, received {}", client->getResponseCode());
  }
  return nullptr;
}

int HttpSiteToSiteClient::readResponse(const std::shared_ptr<sitetosite::Transaction> &transaction, sitetosite::RespondCode &code, std::string &message) {
  if (current_code == sitetosite::FINISH_TRANSACTION) {
    if (transaction->getDirection() == sitetosite::SEND) {
      auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
      if (!stream)
        throw std::runtime_error("Invalid HTTPStream");
      stream->close();
      auto client = stream->getClient();
      if (client->getResponseCode() == 202) {
        code = sitetosite::CONFIRM_TRANSACTION;
        message = std::string(client->getResponseBody().data(), client->getResponseBody().size());
      } else {
        logger_->log_debug("Received response code {}", client->getResponseCode());
        code = sitetosite::UNRECOGNIZED_RESPONSE_CODE;
      }
      return 1;
    } else {
      return 1;
    }
  } else if (transaction->getDirection() == sitetosite::RECEIVE) {
    if (transaction->getState() == sitetosite::TRANSACTION_STARTED || transaction->getState() == sitetosite::DATA_EXCHANGED) {
      if (current_code == sitetosite::CONFIRM_TRANSACTION && transaction->getState() == sitetosite::DATA_EXCHANGED) {
        auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
        if (!stream->isFinished()) {
          logger_->log_debug("confirm read for {}, but not finished ", transaction->getUUIDStr());
          if (stream->waitForDataAvailable()) {
            code = sitetosite::CONTINUE_TRANSACTION;
            return 1;
          }
        }

        code = sitetosite::CONFIRM_TRANSACTION;
      } else {
        auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
        if (stream->isFinished()) {
          logger_->log_debug("Finished {} ", transaction->getUUIDStr());
          code = sitetosite::FINISH_TRANSACTION;
          current_code = sitetosite::FINISH_TRANSACTION;
        } else {
          if (stream->waitForDataAvailable()) {
            logger_->log_debug("data is available, so continuing transaction  {} ", transaction->getUUIDStr());
            code = sitetosite::CONTINUE_TRANSACTION;
          } else {
            logger_->log_debug("No data available for transaction {} ", transaction->getUUIDStr());
            code = sitetosite::FINISH_TRANSACTION;
            current_code = sitetosite::FINISH_TRANSACTION;
          }
        }
      }
    } else if (transaction->getState() == sitetosite::TRANSACTION_CONFIRMED) {
      closeTransaction(transaction->getUUID());
      code = sitetosite::CONFIRM_TRANSACTION;
    }

    return 1;
  } else if (transaction->getState() == sitetosite::TRANSACTION_CONFIRMED) {
    closeTransaction(transaction->getUUID());
    code = sitetosite::TRANSACTION_FINISHED;

    return 1;
  }
  return SiteToSiteClient::readResponse(transaction, code, message);
}
// write respond
int HttpSiteToSiteClient::writeResponse(const std::shared_ptr<sitetosite::Transaction> &transaction, sitetosite::RespondCode code, const std::string& message) {
  current_code = code;
  if (code == sitetosite::CONFIRM_TRANSACTION || code == sitetosite::FINISH_TRANSACTION) {
    return 1;

  } else if (code == sitetosite::CONTINUE_TRANSACTION) {
    logger_->log_debug("Continuing HTTP transaction");
    return 1;
  }
  return SiteToSiteClient::writeResponse(transaction, code, message);
}

bool HttpSiteToSiteClient::getPeerList(std::vector<sitetosite::PeerStatus> &peers) {
  std::stringstream uri;
  uri << getBaseURI() << "site-to-site/peers";

  auto client = create_http_client(uri.str(), http::HttpRequestMethod::GET);

  client->setRequestHeader(PROTOCOL_VERSION_HEADER, "1");

  client->submit();

  if (client->getResponseCode() == 200) {
    if (sitetosite::PeersEntity::parse(logger_, std::string(client->getResponseBody().data(), client->getResponseBody().size()), port_id_, peers)) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<minifi::http::HTTPClient> HttpSiteToSiteClient::openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  std::shared_ptr<minifi::http::HTTPClient> client = create_http_client(uri.str(), http::HttpRequestMethod::POST);
  client->setContentType("application/octet-stream");
  client->setRequestHeader("Accept", "text/plain");
  client->setRequestHeader("Transfer-Encoding", "chunked");
  return client;
}

std::shared_ptr<minifi::http::HTTPClient> HttpSiteToSiteClient::openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  std::shared_ptr<minifi::http::HTTPClient> client = create_http_client(uri.str(), http::HttpRequestMethod::GET);
  return client;
}

//! Transfer string for the process session
bool HttpSiteToSiteClient::transmitPayload(core::ProcessContext&, core::ProcessSession&, const std::string& /*payload*/,
                                           std::map<std::string, std::string> /*attributes*/) {
  return false;
}

void HttpSiteToSiteClient::tearDown() {
  if (peer_state_ >= sitetosite::ESTABLISHED) {
    logger_->log_debug("Site2Site Protocol tearDown");
  }
  known_transactions_.clear();
  peer_->Close();
  peer_state_ = sitetosite::IDLE;
}

void HttpSiteToSiteClient::closeTransaction(const utils::Identifier &transactionID) {
  std::shared_ptr<sitetosite::Transaction> transaction;

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  }

  transaction = it->second;
  if (transaction->closed_) {
    return;
  }

  std::string append_str;
  logger_->log_trace("Site to Site closing transaction {}", transaction->getUUIDStr());


  bool data_received = transaction->getDirection() == sitetosite::RECEIVE && (current_code == sitetosite::CONFIRM_TRANSACTION || current_code == sitetosite::TRANSACTION_FINISHED);

  int code = sitetosite::UNRECOGNIZED_RESPONSE_CODE;
  // In case transaction was used to actually transmit data (conditions are a bit different for send and receive to detect this),
  // it has to be confirmed before closing.
  // In case no data was transmitted, there is nothing to confirm, so the transaction can be cancelled without confirming it.
  // Confirm means matching CRC checksum of data at both sides.
  if (transaction->getState() == sitetosite::TRANSACTION_CONFIRMED || data_received) {
    code = sitetosite::CONFIRM_TRANSACTION;
  } else if (transaction->current_transfers_ == 0 && !transaction->isDataAvailable()) {
    code = sitetosite::CANCEL_TRANSACTION;
  } else {
    std::string directon = transaction->getDirection() == sitetosite::RECEIVE ? "Receive" : "Send";
    logger_->log_error("Transaction {} to be closed is in unexpected state. Direction: {}, tranfers: {}, bytes: {}, state: {}",
        transactionID.to_string(), directon, transaction->total_transfers_, transaction->_bytes, magic_enum::enum_underlying(transaction->getState()));
  }

  std::stringstream uri;
  std::string dir_str = transaction->getDirection() == sitetosite::SEND ? "input-ports" : "output-ports";

  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions/" << transactionID.to_string() << "?responseCode=" << code;

  if (code == sitetosite::CONFIRM_TRANSACTION && data_received) {
    uri << "&checksum=" << transaction->getCRC();
  }

  auto client = create_http_client(uri.str(), http::HttpRequestMethod::DELETE);

  client->setRequestHeader(PROTOCOL_VERSION_HEADER, "1");

  client->setConnectionTimeout(std::chrono::milliseconds(5000));

  client->setRequestHeader("Accept", "application/json");

  client->submit();

  logger_->log_debug("Received {} response code from delete", client->getResponseCode());

  if (client->getResponseCode() == 400) {
    std::string error(client->getResponseBody().data(), client->getResponseBody().size());

    logger_->log_warn("400 received: {}", error);
    std::stringstream message;
    message << "Received " << client->getResponseCode() << " from " << uri.str();
    throw Exception(SITE2SITE_EXCEPTION, message.str());
  }

  transaction->closed_ = true;

  transaction->current_transfers_--;
}

void HttpSiteToSiteClient::deleteTransaction(const utils::Identifier& transactionID) {
  closeTransaction(transactionID);

  SiteToSiteClient::deleteTransaction(transactionID);
}

REGISTER_RESOURCE_AS(HttpSiteToSiteClient, InternalResource, ("HttpSiteToSiteClient", "HttpProtocol"));

}  // namespace org::apache::nifi::minifi::sitetosite
