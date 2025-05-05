/**
 * Site2SiteProtocol class implementation
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
#include <chrono>
#include <utility>
#include <map>
#include <string>
#include <memory>
#include <vector>

#include "sitetosite/RawSiteToSiteClient.h"
#include "io/CRCStream.h"
#include "sitetosite/Peer.h"
#include "utils/gsl.h"
#include "utils/Enum.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::sitetosite {

namespace {
bool negotiateVersion(SiteToSitePeer& peer, const std::string& resource_name, const std::string& negotiation_type, uint32_t& current_version, uint32_t& current_version_index,
    const std::vector<uint32_t>& supported_versions, const std::shared_ptr<core::logging::Logger>& logger) {
  if (const auto ret = peer.write(resource_name); ret == 0 || io::isError(ret)) {
    logger->log_debug("result of writing {} resource name is {}", negotiation_type, ret);
    return false;
  }

  if (const auto ret = peer.write(current_version); ret == 0 || io::isError(ret)) {
    logger->log_debug("result of {} version is {}", negotiation_type, ret);
    return false;
  }

  uint8_t status_code_byte = 0;
  if (const auto ret = peer.read(status_code_byte); ret == 0 || io::isError(ret)) {
    logger->log_debug("result of writing {} version status code  {}", negotiation_type, ret);
    return false;
  }

  auto status_code = magic_enum::enum_cast<ResourceNegotiationStatusCode>(status_code_byte);
  if (!status_code) {
    logger->log_error("Negotiate {} response unknown code {}", negotiation_type, status_code_byte);
    return false;
  }

  switch (*status_code) {
    case ResourceNegotiationStatusCode::RESOURCE_OK: {
      logger->log_debug("Site2Site {} Negotiate version OK", negotiation_type);
      return true;
    }
    case ResourceNegotiationStatusCode::DIFFERENT_RESOURCE_VERSION: {
      uint32_t server_version = 0;
      if (const auto ret = peer.read(server_version); ret == 0 || io::isError(ret)) {
        return false;
      }

      logger->log_info("Site2Site Server Response asked for a different protocol version ", server_version);

      for (uint32_t i = (current_version_index + 1); i < supported_versions.size(); i++) {
        if (server_version >= supported_versions.at(i)) {
          current_version = supported_versions.at(i);
          current_version_index = i;
          return negotiateVersion(peer, resource_name, negotiation_type, current_version, current_version_index, supported_versions, logger);
        }
      }
      logger->log_error("Site2Site Negotiate {} failed to find a common version with server", negotiation_type);
      return false;
    }
    case ResourceNegotiationStatusCode::NEGOTIATED_ABORT: {
      logger->log_error("Site2Site Negotiate {} response ABORT", negotiation_type);
      return false;
    }
    default: {
      logger->log_error("Negotiate {} response unhandled code {}", negotiation_type, magic_enum::enum_name(*status_code));
      return false;
    }
  }
}
}  // namespace

bool RawSiteToSiteClient::establish() {
  if (peer_state_ != PeerState::IDLE) {
    logger_->log_error("Site2Site peer state is not idle while try to establish");
    return false;
  }

  if (auto ret = peer_->open(); !ret) {
    logger_->log_error("Site2Site peer socket open failed");
    return false;
  }

  if (auto ret = initiateResourceNegotiation(); !ret) {
    logger_->log_error("Site2Site Protocol Version Negotiation failed");
    return false;
  }

  logger_->log_debug("Site2Site socket established");
  peer_state_ = PeerState::ESTABLISHED;

  return true;
}

bool RawSiteToSiteClient::initiateResourceNegotiation() {
  if (peer_state_ != PeerState::IDLE) {
    logger_->log_error("Site2Site peer state is not idle while initiateResourceNegotiation");
    return false;
  }

  logger_->log_debug("Negotiate protocol version with destination port {} current version {}", port_id_.to_string(), current_version_);
  return negotiateVersion(*peer_, std::string{PROTOCOL_RESOURCE_NAME}, "protocol", current_version_, current_version_index_, supported_version_, logger_);
}

bool RawSiteToSiteClient::initiateCodecResourceNegotiation() {
  if (peer_state_ != PeerState::HANDSHAKED) {
    logger_->log_error("Site2Site peer state is not handshaked while initiateCodecResourceNegotiation");
    return false;
  }

  logger_->log_trace("Negotiate Codec version with destination port {} current version {}", port_id_.to_string(), current_codec_version_);
  return negotiateVersion(*peer_, std::string{CODEC_RESOURCE_NAME}, "codec", current_codec_version_, current_codec_version_index_, supported_codec_version_, logger_);
}

bool RawSiteToSiteClient::handShake() {
  if (peer_state_ != PeerState::ESTABLISHED) {
    logger_->log_error("Site2Site peer state is not established while handshake");
    return false;
  }
  logger_->log_debug("Site2Site Protocol Perform hand shake with destination port {}", port_id_.to_string());
  comms_identifier_ = utils::IdGenerator::getIdGenerator()->generate();

  if (const auto ret = peer_->write(comms_identifier_); ret == 0 || io::isError(ret)) {
    logger_->log_error("Failed to write comms identifier {}", ret);
    return false;
  }

  std::map<std::string, std::string> properties;
  // TODO(lordgamez): send use_compression_ boolean value when compression support is added
  properties[std::string(magic_enum::enum_name(HandshakeProperty::GZIP))] = "false";
  properties[std::string(magic_enum::enum_name(HandshakeProperty::PORT_IDENTIFIER))] = port_id_.to_string();
  properties[std::string(magic_enum::enum_name(HandshakeProperty::REQUEST_EXPIRATION_MILLIS))] = std::to_string(timeout_.load().count());
  if (current_version_ >= 5) {
    if (batch_count_ > 0) {
      properties[std::string(magic_enum::enum_name(HandshakeProperty::BATCH_COUNT))] = std::to_string(batch_count_);
    }
    if (batch_size_ > 0) {
      properties[std::string(magic_enum::enum_name(HandshakeProperty::BATCH_SIZE))] = std::to_string(batch_size_);
    }
    if (batch_duration_.load() > 0ms) {
      properties[std::string(magic_enum::enum_name(HandshakeProperty::BATCH_DURATION))] = std::to_string(batch_duration_.load().count());
    }
  }

  if (current_version_ >= 3) {
    if (const auto ret = peer_->write(peer_->getURL()); ret == 0 || io::isError(ret)) {
      logger_->log_error("Failed to write peer URL {}", ret);
      return false;
    }
  }

  if (const auto ret = peer_->write(gsl::narrow<uint32_t>(properties.size())); ret == 0 || io::isError(ret)) {
    logger_->log_error("Failed to write properties size {}", ret);
    return false;
  }

  for (const auto& property : properties) {
    if (const auto ret = peer_->write(property.first); ret == 0 || io::isError(ret)) {
      logger_->log_error("Failed to write property key {}", ret);
      return false;
    }

    if (const auto ret = peer_->write(property.second); ret == 0 || io::isError(ret)) {
      logger_->log_error("Failed to write property value {}", ret);
      return false;
    }
    logger_->log_debug("Site2Site Protocol Send handshake properties {} {}", property.first, property.second);
  }

  const auto response = readResponse(nullptr);
  if (!response) {
    return false;
  }

  auto logPortStateError = [this](const std::string& error) {
    logger_->log_error("Site2Site HandShake Failed because destination port, {}, is {}", port_id_.to_string(), error);
  };

  switch (response->code) {
    case ResponseCode::PROPERTIES_OK:
      logger_->log_debug("Site2Site HandShake Completed");
      peer_state_ = PeerState::HANDSHAKED;
      return true;
    case ResponseCode::PORT_NOT_IN_VALID_STATE:
      logPortStateError("in invalid state");
      return false;
    case ResponseCode::UNKNOWN_PORT:
      logPortStateError("an unknown port");
      return false;
    case ResponseCode::PORTS_DESTINATION_FULL:
      logPortStateError("full");
      return false;
    case ResponseCode::UNAUTHORIZED:
      logger_->log_error("Site2Site HandShake on port {} failed: UNAUTHORIZED", port_id_.to_string());
      return false;
    default:
      logger_->log_error("Site2Site HandShake on port {} failed: unknown response code {}", port_id_.to_string(), magic_enum::enum_underlying(response->code));
      return false;
  }
}

void RawSiteToSiteClient::tearDown() {
  if (magic_enum::enum_underlying(peer_state_) >= magic_enum::enum_underlying(PeerState::ESTABLISHED)) {
    logger_->log_trace("Site2Site Protocol tearDown");
    writeRequestType(RequestType::SHUTDOWN);
  }

  known_transactions_.clear();
  peer_->close();
  peer_state_ = PeerState::IDLE;
}

std::optional<std::vector<PeerStatus>> RawSiteToSiteClient::getPeerList() {
  if (!establish() || !handShake()) {
    tearDown();
    return std::nullopt;
  }

  if (writeRequestType(RequestType::REQUEST_PEER_LIST) <= 0) {
    tearDown();
    return std::nullopt;
  }

  uint32_t number_of_peers = 0;
  std::vector<PeerStatus> peers;
  if (const auto ret = peer_->read(number_of_peers); ret == 0 || io::isError(ret)) {
    tearDown();
    return std::nullopt;
  }

  for (uint32_t i = 0; i < number_of_peers; i++) {
    std::string host;
    if (const auto ret = peer_->read(host); ret == 0 || io::isError(ret)) {
      tearDown();
      return std::nullopt;
    }

    uint32_t port = 0;
    if (const auto ret = peer_->read(port); ret == 0 || io::isError(ret)) {
      tearDown();
      return std::nullopt;
    }

    uint8_t secure = 0;
    if (const auto ret = peer_->read(secure); ret == 0 || io::isError(ret)) {
      tearDown();
      return std::nullopt;
    }

    uint32_t count = 0;
    if (const auto ret = peer_->read(count); ret == 0 || io::isError(ret)) {
      tearDown();
      return std::nullopt;
    }

    peers.push_back(PeerStatus(port_id_, host, gsl::narrow<uint16_t>(port), count, true));
    logger_->log_trace("Site2Site Peer host {} port {} Secure {}", host, port, std::to_string(secure));
  }

  tearDown();
  return peers;
}

int64_t RawSiteToSiteClient::writeRequestType(RequestType type) {
  const auto write_result = peer_->write(std::string{magic_enum::enum_name(type)});
  return io::isError(write_result) ? -1 : gsl::narrow<int64_t>(write_result);
}

bool RawSiteToSiteClient::negotiateCodec() {
  if (peer_state_ != PeerState::HANDSHAKED) {
    logger_->log_error("Site2Site peer state is not handshaked while negotiate codec");
    return false;
  }

  logger_->log_trace("Site2Site Protocol Negotiate Codec with destination port {}", port_id_.to_string());

  if (writeRequestType(RequestType::NEGOTIATE_FLOWFILE_CODEC) <= 0) {
    return false;
  }

  if (!initiateCodecResourceNegotiation()) {
    logger_->log_error("Site2Site Codec Version Negotiation failed");
    return false;
  }

  logger_->log_trace("Site2Site Codec Completed and move to READY state for data transfer");
  peer_state_ = PeerState::READY;

  return true;
}

bool RawSiteToSiteClient::bootstrap() {
  if (peer_state_ == PeerState::READY) {
    return true;
  }

  tearDown();

  if (!establish() || !handShake() || !negotiateCodec()) {
    tearDown();
    return false;
  }

  logger_->log_debug("Site to Site ready for data transaction");
  return true;
}

std::shared_ptr<Transaction> RawSiteToSiteClient::createTransaction(TransferDirection direction) {
  if (peer_state_ != PeerState::READY) {
    bootstrap();
  }

  std::shared_ptr<Transaction> transaction = nullptr;
  if (peer_state_ != PeerState::READY) {
    return transaction;
  }

  if (direction == TransferDirection::RECEIVE) {
    if (writeRequestType(RequestType::RECEIVE_FLOWFILES) <= 0) {
      return transaction;
    }

    auto response = readResponse(nullptr);
    if (!response) {
      return transaction;
    }

    org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(gsl::make_not_null(peer_.get()));
    switch (response->code) {
      case ResponseCode::MORE_DATA:
        logger_->log_trace("Site2Site peer indicates that data is available");
        transaction = std::make_shared<Transaction>(direction, std::move(crcstream));
        known_transactions_[transaction->getUUID()] = transaction;
        transaction->setDataAvailable(true);
        logger_->log_trace("Site2Site create transaction {}", transaction->getUUIDStr());
        return transaction;
      case ResponseCode::NO_MORE_DATA:
        logger_->log_trace("Site2Site peer indicates that no data is available");
        transaction = std::make_shared<Transaction>(direction, std::move(crcstream));
        known_transactions_[transaction->getUUID()] = transaction;
        transaction->setDataAvailable(false);
        logger_->log_trace("Site2Site create transaction {}", transaction->getUUIDStr());
        return transaction;
      default:
        logger_->log_warn("Site2Site got unexpected response {} when asking for data", magic_enum::enum_underlying(response->code));
        return nullptr;
    }
  } else {
    if (writeRequestType(RequestType::SEND_FLOWFILES) <= 0) {
      return nullptr;
    }

    org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(gsl::make_not_null(peer_.get()));
    transaction = std::make_shared<Transaction>(direction, std::move(crcstream));
    known_transactions_[transaction->getUUID()] = transaction;
    logger_->log_trace("Site2Site create transaction {}", transaction->getUUIDStr());
    return transaction;
  }
}

bool RawSiteToSiteClient::transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload, const std::map<std::string, std::string>& attributes) {
  if (payload.length() <= 0) {
    return false;
  }

  if (peer_state_ != PeerState::READY && !bootstrap()) {
    return false;
  }

  if (peer_state_ != PeerState::READY) {
    context.yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
  }

  auto transaction = createTransaction(TransferDirection::SEND);
  if (transaction == nullptr) {
    context.yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  utils::Identifier transaction_id = transaction->getUUID();
  try {
    DataPacket packet(transaction, attributes, payload);

    if (!send(transaction_id, &packet, nullptr, &session)) {
      throw Exception(SITE2SITE_EXCEPTION, "Send Failed in transaction " + transaction_id.to_string());
    }
    logger_->log_info("Site2Site transaction {} sent bytes length", transaction_id.to_string(), payload.length());

    if (!confirm(transaction_id)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed in transaction " + transaction_id.to_string());
    }
    if (!complete(context, transaction_id)) {
      throw Exception(SITE2SITE_EXCEPTION, "Complete Failed in transaction " + transaction_id.to_string());
    }
    logger_->log_info("Site2Site transaction {} successfully send flow record {} content bytes {}", transaction_id.to_string(), transaction->getCurrentTransfers(), transaction->getBytes());
  } catch (const std::exception &exception) {
    handleTransactionError(transaction, context, exception);
    throw;
  }

  deleteTransaction(transaction_id);

  return true;
}

}  // namespace org::apache::nifi::minifi::sitetosite
