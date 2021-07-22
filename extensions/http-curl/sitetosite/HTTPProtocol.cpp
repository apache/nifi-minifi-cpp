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
#include "HTTPProtocol.h"

#include <stdio.h>
#include <chrono>
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <random>
#include <iostream>
#include <vector>

#include "PeersEntity.h"
#include "io/CRCStream.h"
#include "sitetosite/Peer.h"
#include "io/validation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

std::shared_ptr<utils::IdGenerator> HttpSiteToSiteClient::id_generator_ = utils::IdGenerator::getIdGenerator();

std::optional<utils::Identifier> HttpSiteToSiteClient::parseTransactionId(const std::string &uri) {
  const size_t last_slash_pos = uri.find_last_of('/');
  size_t id_start_pos = 0;
  if (last_slash_pos != std::string::npos) {
    id_start_pos = last_slash_pos + 1;
  }
  return utils::Identifier::parse(uri.substr(id_start_pos));
}

std::shared_ptr<Transaction> HttpSiteToSiteClient::createTransaction(TransferDirection direction) {
  std::string dir_str = direction == SEND ? "input-ports" : "output-ports";
  std::stringstream uri;
  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions";
  auto client = create_http_client(uri.str(), "POST");
  client->appendHeader(PROTOCOL_VERSION_HEADER, "1");
  client->setConnectionTimeout(std::chrono::milliseconds(5000));
  client->setContentType("application/json");
  client->appendHeader("Accept: application/json");
  client->setUseChunkedEncoding();
  client->setPostFields("");
  client->submit();
  if (peer_->getStream() != nullptr)
    logger_->log_debug("Closing %s", ((io::HttpStream*) peer_->getStream())->getClientRef()->getURL());
  if (client->getResponseCode() == 201) {
    // parse the headers
    auto intent_name = client->getHeaderValue("x-location-uri-intent");
    if (utils::StringUtils::equalsIgnoreCase(intent_name, "transaction-url")) {
      auto url = client->getHeaderValue("Location");

      if (IsNullOrEmpty(url)) {
        logger_->log_debug("Location is empty");
      } else {
        org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(gsl::make_not_null(peer_.get()));
        auto transaction = std::make_shared<HttpTransaction>(direction, std::move(crcstream));
        transaction->initialize(this, url);
        auto transactionId = parseTransactionId(url);
        if (!transactionId)
          return nullptr;
        transaction->setTransactionId(transactionId.value());
        std::shared_ptr<minifi::utils::HTTPClient> client;
        if (transaction->getDirection() == SEND) {
          client = openConnectionForSending(transaction);
        } else {
          client = openConnectionForReceive(transaction);
          transaction->setDataAvailable(true);
          // a 201 tells us that data is available. A 200 would mean that nothing is available.
        }

        client->appendHeader(PROTOCOL_VERSION_HEADER, "1");
        peer_->setStream(std::unique_ptr<io::BaseStream>(new io::HttpStream(client)));
        logger_->log_debug("Created transaction id -%s-", transaction->getUUID().to_string());
        known_transactions_[transaction->getUUID()] = transaction;
        return transaction;
      }
    } else {
      logger_->log_debug("Could not create transaction, intent is %s", intent_name);
    }
  } else {
    peer_->setStream(nullptr);
    logger_->log_debug("Could not create transaction, received %d", client->getResponseCode());
  }
  return nullptr;
}

int HttpSiteToSiteClient::readResponse(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message) {
  if (current_code == FINISH_TRANSACTION) {
    if (transaction->getDirection() == SEND) {
      auto stream = dynamic_cast<io::HttpStream*>(peer_->getStream());
      stream->close();
      auto client = stream->getClient();
      if (client->getResponseCode() == 202) {
        code = CONFIRM_TRANSACTION;
        message = std::string(client->getResponseBody().data(), client->getResponseBody().size());
      } else {
        logger_->log_debug("Received response code %d", client->getResponseCode());
        code = UNRECOGNIZED_RESPONSE_CODE;
      }
      return 1;
    } else {
      return 1;
    }
  } else if (transaction->getDirection() == RECEIVE) {
    if (transaction->getState() == TRANSACTION_STARTED || transaction->getState() == DATA_EXCHANGED) {
      if (current_code == CONFIRM_TRANSACTION && transaction->getState() == DATA_EXCHANGED) {
        auto stream = dynamic_cast<io::HttpStream*>(peer_->getStream());
        if (!stream->isFinished()) {
          logger_->log_debug("confirm read for %s, but not finished ", transaction->getUUIDStr());
          if (stream->waitForDataAvailable()) {
            code = CONTINUE_TRANSACTION;
            return 1;
          }
        }

        code = CONFIRM_TRANSACTION;
      } else {
        auto stream = dynamic_cast<io::HttpStream*>(peer_->getStream());
        if (stream->isFinished()) {
          logger_->log_debug("Finished %s ", transaction->getUUIDStr());
          code = FINISH_TRANSACTION;
          current_code = FINISH_TRANSACTION;
        } else {
          if (stream->waitForDataAvailable()) {
            logger_->log_debug("data is available, so continuing transaction  %s ", transaction->getUUIDStr());
            code = CONTINUE_TRANSACTION;
          } else {
            logger_->log_debug("No data available for transaction %s ", transaction->getUUIDStr());
            code = FINISH_TRANSACTION;
            current_code = FINISH_TRANSACTION;
          }
        }
      }
    } else if (transaction->getState() == TRANSACTION_CONFIRMED) {
      closeTransaction(transaction->getUUID());
      code = CONFIRM_TRANSACTION;
    }

    return 1;
  } else if (transaction->getState() == TRANSACTION_CONFIRMED) {
    closeTransaction(transaction->getUUID());
    code = TRANSACTION_FINISHED;

    return 1;
  }
  return SiteToSiteClient::readResponse(transaction, code, message);
}
// write respond
int HttpSiteToSiteClient::writeResponse(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message) {
  current_code = code;
  if (code == CONFIRM_TRANSACTION || code == FINISH_TRANSACTION) {
    return 1;

  } else if (code == CONTINUE_TRANSACTION) {
    logger_->log_debug("Continuing HTTP transaction");
    return 1;
  }
  return SiteToSiteClient::writeResponse(transaction, code, message);
}

bool HttpSiteToSiteClient::getPeerList(std::vector<PeerStatus> &peers) {
  std::stringstream uri;
  uri << getBaseURI() << "site-to-site/peers";

  auto client = create_http_client(uri.str(), "GET");

  client->appendHeader(PROTOCOL_VERSION_HEADER, "1");

  client->submit();

  if (client->getResponseCode() == 200) {
    if (sitetosite::PeersEntity::parse(logger_, std::string(client->getResponseBody().data(), client->getResponseBody().size()), port_id_, peers)) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<minifi::utils::HTTPClient> HttpSiteToSiteClient::openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  std::shared_ptr<minifi::utils::HTTPClient> client = create_http_client(uri.str(), "POST");
  client->setContentType("application/octet-stream");
  client->appendHeader("Accept", "text/plain");
  client->setUseChunkedEncoding();
  return client;
}

std::shared_ptr<minifi::utils::HTTPClient> HttpSiteToSiteClient::openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  std::shared_ptr<minifi::utils::HTTPClient> client = create_http_client(uri.str(), "GET");
  return client;
}

//! Transfer string for the process session
bool HttpSiteToSiteClient::transmitPayload(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& /*session*/, const std::string& /*payload*/,
                                           std::map<std::string, std::string> /*attributes*/) {
  return false;
}

void HttpSiteToSiteClient::tearDown() {
  if (peer_state_ >= ESTABLISHED) {
    logger_->log_debug("Site2Site Protocol tearDown");
  }
  known_transactions_.clear();
  peer_->Close();
  peer_state_ = IDLE;
}

void HttpSiteToSiteClient::closeTransaction(const utils::Identifier &transactionID) {
  std::shared_ptr<Transaction> transaction = NULL;

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  }

  transaction = it->second;
  if (transaction->closed_) {
    return;
  }

  std::string append_str;
  logger_->log_trace("Site to Site closing transaction %s", transaction->getUUIDStr());


  bool data_received = transaction->getDirection() == RECEIVE && (current_code == CONFIRM_TRANSACTION || current_code == TRANSACTION_FINISHED);

  int code = UNRECOGNIZED_RESPONSE_CODE;
  // In case transaction was used to actually transmit data (conditions are a bit different for send and receive to detect this),
  // it has to be confirmed before closing.
  // In case no data was transmitted, there is nothing to confirm, so the transaction can be cancelled without confirming it.
  // Confirm means matching CRC checksum of data at both sides.
  if (transaction->getState() == TRANSACTION_CONFIRMED || data_received) {
    code = CONFIRM_TRANSACTION;
  } else if (transaction->current_transfers_ == 0 && !transaction->isDataAvailable()) {
    code = CANCEL_TRANSACTION;
  } else {
    std::string directon = transaction->getDirection() == RECEIVE ? "Receive" : "Send";
    logger_->log_error("Transaction %s to be closed is in unexpected state. Direction: %s, tranfers: %d, bytes: %llu, state: %d",
        transactionID.to_string(), directon, transaction->total_transfers_, transaction->_bytes, transaction->getState());
  }

  std::stringstream uri;
  std::string dir_str = transaction->getDirection() == SEND ? "input-ports" : "output-ports";

  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions/" << transactionID.to_string() << "?responseCode=" << code;

  if (code == CONFIRM_TRANSACTION && data_received) {
    uri << "&checksum=" << transaction->getCRC();
  }

  auto client = create_http_client(uri.str(), "DELETE");

  client->appendHeader(PROTOCOL_VERSION_HEADER, "1");

  client->setConnectionTimeout(std::chrono::milliseconds(5000));

  client->appendHeader("Accept", "application/json");

  client->submit();

  logger_->log_debug("Received %d response code from delete", client->getResponseCode());

  if (client->getResponseCode() == 400) {
    std::string error(client->getResponseBody().data(), client->getResponseBody().size());

    logging::LOG_WARN(logger_) << "400 received: " << error;
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

REGISTER_INTERNAL_RESOURCE_AS(HttpSiteToSiteClient, ("HttpSiteToSiteClient", "HttpProtocol"));

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
