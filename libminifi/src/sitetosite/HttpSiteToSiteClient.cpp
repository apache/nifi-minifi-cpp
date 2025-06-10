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
#include "sitetosite/HttpSiteToSiteClient.h"

#include <chrono>
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <vector>
#include <optional>

#include "io/CRCStream.h"
#include "sitetosite/Peer.h"
#include "io/validation.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "Exception.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#undef DELETE  // macro on windows

namespace org::apache::nifi::minifi::sitetosite {

namespace {
std::optional<utils::Identifier> parseTransactionId(const std::string &uri) {
  return utils::Identifier::parse(utils::string::partAfterLastOccurrenceOf(uri, '/'));
}

std::optional<std::vector<PeerStatus>> parsePeerStatuses(const std::shared_ptr<core::logging::Logger> &logger, const std::string &entity, const utils::Identifier &id) {
  try {
    rapidjson::Document root;
    rapidjson::ParseResult ok = root.Parse(entity.c_str());
    if (!ok) {
      std::stringstream ss;
      ss << "Failed to parse archive lens stack from JSON string with reason: "
          << rapidjson::GetParseError_En(ok.Code())
          << " at offset " << ok.Offset();

      throw Exception(ExceptionType::GENERAL_EXCEPTION, ss.str());
    }

    std::vector<PeerStatus> peer_statuses;
    if (!root.HasMember("peers") || !root["peers"].IsArray() || root["peers"].Size() <= 0) {
      logger->log_debug("Peers is either not a member or is empty. String to analyze: {}", entity);
      return peer_statuses;
    }

    for (const auto &peer : root["peers"].GetArray()) {
      std::string hostname;
      int port = 0;
      int flow_file_count = 0;

      if (peer.HasMember("hostname") && peer["hostname"].IsString() &&
          peer.HasMember("port") && peer["port"].IsNumber()) {
        hostname = peer["hostname"].GetString();
        port = peer["port"].GetInt();
      }

      if (peer.HasMember("flowFileCount")) {
        if (peer["flowFileCount"].IsNumber()) {
          flow_file_count = gsl::narrow<int>(peer["flowFileCount"].GetInt64());
        } else {
          logger->log_debug("Could not parse flowFileCount, so we're going to continue without it");
        }
      }

      // host name and port are required.
      if (!IsNullOrEmpty(hostname) && port > 0) {
        PeerStatus status(id, hostname, port, flow_file_count, true);
        peer_statuses.push_back(std::move(status));
      } else {
        logger->log_debug("hostname empty or port is zero. hostname: {}, port: {}", hostname, port);
      }
    }
    return peer_statuses;
  } catch (const Exception &exception) {
    logger->log_debug("Caught Exception {}", exception.what());
    return std::nullopt;
  }
}
}  // namespace

std::shared_ptr<Transaction> HttpSiteToSiteClient::createTransaction(TransferDirection direction) {
  std::string dir_str = direction == TransferDirection::SEND ? "input-ports" : "output-ports";
  std::stringstream uri;
  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions";
  auto client = createHttpClient(uri.str(), http::HttpRequestMethod::POST);
  setSiteToSiteHeaders(*client);
  client->setConnectionTimeout(std::chrono::milliseconds(5000));
  client->setContentType("application/json");
  client->setRequestHeader("Accept", "application/json");
  client->setRequestHeader("Transfer-Encoding", "chunked");
  client->setPostFields("");
  client->submit();

  if (auto http_stream = dynamic_cast<http::HttpStream*>(peer_->getStream())) {
    logger_->log_debug("Closing {}", http_stream->getClientRef()->getURL());
  }

  if (client->getResponseCode() != 201) {
    peer_->setStream(nullptr);
    logger_->log_debug("Could not create transaction, received {}", client->getResponseCode());
    return nullptr;
  }
  // parse the headers
  auto intent_name = client->getHeaderValue("x-location-uri-intent");
  if (!utils::string::equalsIgnoreCase(intent_name, "transaction-url")) {
    logger_->log_debug("Could not create transaction, intent is {}", intent_name);
    return nullptr;
  }

  auto url = client->getHeaderValue("Location");
  if (IsNullOrEmpty(url)) {
    logger_->log_debug("Location is empty");
    return nullptr;
  }

  org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(gsl::make_not_null(peer_.get()));
  auto transaction = std::make_shared<HttpTransaction>(direction, std::move(crcstream));
  transaction->initialize(this, url);
  auto transaction_id = parseTransactionId(url);
  if (!transaction_id) {
    logger_->log_debug("Transaction ID is empty");
    return nullptr;
  }
  transaction->setTransactionId(transaction_id.value());
  std::shared_ptr<minifi::http::HTTPClient> transaction_client;
  if (transaction->getDirection() == TransferDirection::SEND) {
    transaction_client = openConnectionForSending(transaction);
  } else {
    transaction_client = openConnectionForReceive(transaction);
    transaction->setDataAvailable(true);
    // 201 tells us that data is available. 200 would mean that nothing is available.
  }
  gsl_Assert(transaction_client);

  setSiteToSiteHeaders(*transaction_client);
  peer_->setStream(std::make_unique<http::HttpStream>(transaction_client));
  logger_->log_debug("Created transaction id -{}-", transaction->getUUID().to_string());
  known_transactions_[transaction->getUUID()] = transaction;
  return transaction;
}

std::optional<SiteToSiteResponse> HttpSiteToSiteClient::readResponseForReceiveTransfer(const std::shared_ptr<Transaction>& transaction) {
  SiteToSiteResponse response;
  if (current_code_ == ResponseCode::FINISH_TRANSACTION) {
    return response;
  }

  if (transaction->getState() == TransactionState::TRANSACTION_STARTED || transaction->getState() == TransactionState::DATA_EXCHANGED) {
    if (current_code_ == ResponseCode::CONFIRM_TRANSACTION && transaction->getState() == TransactionState::DATA_EXCHANGED) {
      auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
      if (!stream->isFinished()) {
        logger_->log_debug("confirm read for {}, but not finished ", transaction->getUUIDStr());
        if (stream->waitForDataAvailable()) {
          response.code = ResponseCode::CONTINUE_TRANSACTION;
          return response;
        }
      }

      response.code = ResponseCode::CONFIRM_TRANSACTION;
      return response;
    }

    auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
    if (stream->isFinished()) {
      logger_->log_debug("Finished {} ", transaction->getUUIDStr());
      response.code = ResponseCode::FINISH_TRANSACTION;
      current_code_ = ResponseCode::FINISH_TRANSACTION;
      return response;
    }

    if (stream->waitForDataAvailable()) {
      logger_->log_debug("data is available, so continuing transaction  {} ", transaction->getUUIDStr());
      response.code = ResponseCode::CONTINUE_TRANSACTION;
      return response;
    }

    logger_->log_debug("No data available for transaction {} ", transaction->getUUIDStr());
    response.code = ResponseCode::FINISH_TRANSACTION;
    current_code_ = ResponseCode::FINISH_TRANSACTION;
    return response;
  }

  if (transaction->getState() == TransactionState::TRANSACTION_CONFIRMED) {
    closeTransaction(transaction->getUUID());
    response.code = ResponseCode::CONFIRM_TRANSACTION;
    return response;
  }

  return response;
}

std::optional<SiteToSiteResponse> HttpSiteToSiteClient::readResponseForSendTransfer(const std::shared_ptr<Transaction>& transaction) {
  SiteToSiteResponse response;
  if (current_code_ == ResponseCode::FINISH_TRANSACTION) {
    auto stream = dynamic_cast<http::HttpStream*>(peer_->getStream());
    if (!stream) {
      throw std::runtime_error("Invalid HTTPStream");
    }

    stream->close();
    auto client = stream->getClient();
    if (client->getResponseCode() == 202) {
      response.code = ResponseCode::CONFIRM_TRANSACTION;
      response.message = std::string(client->getResponseBody().data(), client->getResponseBody().size());
      return response;
    }

    logger_->log_debug("Received response code {}", client->getResponseCode());
    response.code = ResponseCode::UNRECOGNIZED_RESPONSE_CODE;
    return response;
  }

  if (transaction->getState() == TransactionState::TRANSACTION_CONFIRMED) {
    closeTransaction(transaction->getUUID());
    response.code = ResponseCode::TRANSACTION_FINISHED;
    return response;
  }

  return SiteToSiteClient::readResponse(transaction);
}

std::optional<SiteToSiteResponse> HttpSiteToSiteClient::readResponse(const std::shared_ptr<Transaction>& transaction) {
  if (transaction->getDirection() == TransferDirection::RECEIVE) {
    return readResponseForReceiveTransfer(transaction);
  }

  return readResponseForSendTransfer(transaction);
}

bool HttpSiteToSiteClient::writeResponse(const std::shared_ptr<Transaction> &transaction, const SiteToSiteResponse& response) {
  current_code_ = response.code;
  if (response.code == ResponseCode::CONFIRM_TRANSACTION || response.code == ResponseCode::FINISH_TRANSACTION) {
    return true;
  } else if (response.code == ResponseCode::CONTINUE_TRANSACTION) {
    logger_->log_debug("Continuing HTTP transaction");
    return true;
  }
  return SiteToSiteClient::writeResponse(transaction, response);
}

std::optional<std::vector<PeerStatus>> HttpSiteToSiteClient::getPeerList() {
  std::stringstream uri;
  uri << getBaseURI() << "site-to-site/peers";

  auto client = createHttpClient(uri.str(), http::HttpRequestMethod::GET);

  setSiteToSiteHeaders(*client);

  client->submit();

  if (client->getResponseCode() == 200) {
    return parsePeerStatuses(logger_, std::string(client->getResponseBody().data(), client->getResponseBody().size()), port_id_);
  }
  return std::nullopt;
}

std::shared_ptr<minifi::http::HTTPClient> HttpSiteToSiteClient::openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  std::shared_ptr<minifi::http::HTTPClient> client = createHttpClient(uri.str(), http::HttpRequestMethod::POST);
  client->setContentType("application/octet-stream");
  client->setRequestHeader("Accept", "text/plain");
  client->setRequestHeader("Transfer-Encoding", "chunked");
  return client;
}

std::shared_ptr<minifi::http::HTTPClient> HttpSiteToSiteClient::openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction) {
  std::stringstream uri;
  uri << transaction->getTransactionUrl() << "/flow-files";
  return createHttpClient(uri.str(), http::HttpRequestMethod::GET);
}

std::string HttpSiteToSiteClient::getBaseURI() {
  std::string uri = ssl_context_service_ != nullptr ? "https://" : "http://";
  uri.append(peer_->getHostName());
  uri.append(":");
  uri.append(std::to_string(peer_->getPort()));
  uri.append("/nifi-api/");
  return uri;
}

std::unique_ptr<minifi::http::HTTPClient> HttpSiteToSiteClient::createHttpClient(const std::string &uri, http::HttpRequestMethod method) {
  auto http_client_ = std::make_unique<minifi::http::HTTPClient>(uri, ssl_context_service_);
  http_client_->initialize(method, uri, ssl_context_service_);
  if (!peer_->getInterface().empty()) {
    logger_->log_info("HTTP Site2Site bind local network interface {}", peer_->getInterface());
    http_client_->setInterface(peer_->getInterface());
  }
  if (!peer_->getHTTPProxy().host.empty()) {
    logger_->log_info("HTTP Site2Site setup http proxy host {}", peer_->getHTTPProxy().host);
    http_client_->setHTTPProxy(peer_->getHTTPProxy());
  }
  http_client_->setReadTimeout(idle_timeout_);
  return http_client_;
}

bool HttpSiteToSiteClient::transmitPayload(core::ProcessContext&, const std::string& /*payload*/, const std::map<std::string, std::string>& /*attributes*/) {
  return false;
}

void HttpSiteToSiteClient::tearDown() {
  if (magic_enum::enum_underlying(peer_state_) >= magic_enum::enum_underlying(PeerState::ESTABLISHED)) {
    logger_->log_debug("Site2Site Protocol tearDown");
  }
  known_transactions_.clear();
  peer_->close();
  peer_state_ = PeerState::IDLE;
}

void HttpSiteToSiteClient::closeTransaction(const utils::Identifier &transaction_id) {
  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    return;
  }

  auto transaction = it->second;
  if (transaction->isClosed()) {
    return;
  }

  logger_->log_trace("Site to Site closing transaction {}", transaction->getUUIDStr());

  bool data_received = transaction->getDirection() == TransferDirection::RECEIVE && (current_code_ == ResponseCode::CONFIRM_TRANSACTION || current_code_ == ResponseCode::TRANSACTION_FINISHED);

  auto code = ResponseCode::UNRECOGNIZED_RESPONSE_CODE;
  // In case transaction was used to actually transmit data (conditions are a bit different for send and receive to detect this),
  // it has to be confirmed before closing.
  // In case no data was transmitted, there is nothing to confirm, so the transaction can be cancelled without confirming it.
  // Confirm means matching CRC checksum of data at both sides.
  if (transaction->getState() == TransactionState::TRANSACTION_CONFIRMED || data_received) {
    code = ResponseCode::CONFIRM_TRANSACTION;
  } else if (transaction->getCurrentTransfers() == 0 && !transaction->isDataAvailable()) {
    code = ResponseCode::CANCEL_TRANSACTION;
  } else {
    std::string directon = transaction->getDirection() == TransferDirection::RECEIVE ? "Receive" : "Send";
    logger_->log_error("Transaction {} to be closed is in unexpected state. Direction: {}, transfers: {}, bytes: {}, state: {}",
        transaction_id.to_string(), directon, transaction->getTotalTransfers(), transaction->getBytes(), magic_enum::enum_underlying(transaction->getState()));
  }

  std::stringstream uri;
  std::string dir_str = transaction->getDirection() == TransferDirection::SEND ? "input-ports" : "output-ports";

  uri << getBaseURI() << "data-transfer/" << dir_str << "/" << getPortId().to_string() << "/transactions/" << transaction_id.to_string() <<
    "?responseCode=" << std::to_string(magic_enum::enum_underlying(code));

  if (code == ResponseCode::CONFIRM_TRANSACTION && data_received) {
    uri << "&checksum=" << transaction->getCRC();
  }

  auto client = createHttpClient(uri.str(), http::HttpRequestMethod::DELETE);
  setSiteToSiteHeaders(*client);
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

  transaction->close();
  transaction->decrementCurrentTransfers();
}

void HttpSiteToSiteClient::deleteTransaction(const utils::Identifier& transaction_id) {
  closeTransaction(transaction_id);

  SiteToSiteClient::deleteTransaction(transaction_id);
}

void HttpSiteToSiteClient::setSiteToSiteHeaders(minifi::http::HTTPClient& client) {
  client.setRequestHeader(PROTOCOL_VERSION_HEADER, "1");
  // TODO(lordgamez): send use_compression_ boolean value when compression support is added
  client.setRequestHeader(HANDSHAKE_PROPERTY_USE_COMPRESSION, "false");
  if (timeout_.load() > 0ms) {
    client.setRequestHeader(HANDSHAKE_PROPERTY_REQUEST_EXPIRATION, std::to_string(timeout_.load().count()));
  }
  if (batch_count_ > 0) {
    client.setRequestHeader(HANDSHAKE_PROPERTY_BATCH_COUNT, std::to_string(batch_count_));
  }
  if (batch_size_ > 0) {
    client.setRequestHeader(HANDSHAKE_PROPERTY_BATCH_SIZE, std::to_string(batch_size_));
  }
  if (batch_duration_.load() > std::chrono::milliseconds(0)) {
    client.setRequestHeader(HANDSHAKE_PROPERTY_BATCH_DURATION, std::to_string(batch_duration_.load().count()));
  }
}

}  // namespace org::apache::nifi::minifi::sitetosite
