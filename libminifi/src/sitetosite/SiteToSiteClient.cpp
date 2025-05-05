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
#include "sitetosite/SiteToSiteClient.h"

#include <map>
#include <string>
#include <memory>

#include "utils/gsl.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::sitetosite {

std::optional<SiteToSiteResponse> SiteToSiteClient::readResponse(const std::shared_ptr<Transaction>& /*transaction*/) {
  uint8_t result_byte = 0;
  if (const auto ret = peer_->read(result_byte); ret == 0 || io::isError(ret) || result_byte != CODE_SEQUENCE_VALUE_1) {
    logger_->log_error("Site2Site read response failed: invalid code sequence 1 value");
    return std::nullopt;
  }

  if (const auto ret = peer_->read(result_byte); ret == 0 || io::isError(ret) || result_byte != CODE_SEQUENCE_VALUE_2) {
    logger_->log_error("Site2Site read response failed: invalid code sequence 2 value");
    return std::nullopt;
  }

  if (const auto ret = peer_->read(result_byte); ret == 0 || io::isError(ret)) {
    logger_->log_error("Site2Site read response failed: failed to read response code");
    return std::nullopt;
  }

  SiteToSiteResponse response;
  if (auto code = magic_enum::enum_cast<ResponseCode>(result_byte)) {
    response.code = *code;
  } else {
    logger_->log_error("Site2Site read response failed: invalid response code");
    return std::nullopt;
  }

  const ResponseCodeContext* response_code_context = getResponseCodeContext(response.code);
  if (!response_code_context) {
    logger_->log_error("Site2Site read response failed: invalid response code context");
    return std::nullopt;
  }
  if (response_code_context->has_description) {
    if (const auto ret = peer_->read(response.message); ret == 0 || io::isError(ret)) {
      logger_->log_error("Site2Site read response failed: failed to read response message");
      return std::nullopt;
    }
  }
  return response;
}

void SiteToSiteClient::handleTransactionError(const std::shared_ptr<Transaction>& transaction, core::ProcessContext& context, const std::exception& exception) {
  if (transaction) {
    deleteTransaction(transaction->getUUID());
  }
  context.yield();
  tearDown();
  logger_->log_warn("Caught Exception, type: {}, what: {}", typeid(exception).name(), exception.what());
}

void SiteToSiteClient::deleteTransaction(const utils::Identifier& transaction_id) {
  std::shared_ptr<Transaction> transaction;

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    logger_->log_warn("Site2Site transaction id '{}' not found for delete", transaction_id.to_string());
    return;
  } else {
    transaction = it->second;
  }

  logger_->log_debug("Site2Site delete transaction {}", transaction->getUUIDStr());
  known_transactions_.erase(transaction_id);
}

bool SiteToSiteClient::writeResponse(const std::shared_ptr<Transaction>& /*transaction*/, const SiteToSiteResponse& response) {
  const ResponseCodeContext* response_code_context = getResponseCodeContext(response.code);
  if (!response_code_context) {
    return false;
  }

  const std::array<uint8_t, 3> code_sequence = { CODE_SEQUENCE_VALUE_1, CODE_SEQUENCE_VALUE_2, magic_enum::enum_underlying(response.code) };
  const auto ret = peer_->write(code_sequence.data(), 3);
  if (ret != 3) {
    return false;
  }

  if (response_code_context->has_description) {
    return !(io::isError(peer_->write(response.message)));
  }
  return true;
}

bool SiteToSiteClient::transferFlowFiles(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow = session.get();
  if (!flow) {
    return false;
  }

  if (peer_state_ != PeerState::READY) {
    if (!bootstrap()) {
      return false;
    }
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
  std::chrono::high_resolution_clock::time_point transaction_started_at = std::chrono::high_resolution_clock::now();

  try {
    while (true) {
      auto start_time = std::chrono::steady_clock::now();
      std::string payload;
      DataPacket packet(transaction, flow->getAttributes(), payload);

      if (!send(transaction_id, &packet, flow, &session)) {
        throw Exception(SITE2SITE_EXCEPTION, "Send Failed");
      }

      logger_->log_debug("Site2Site transaction {} send flow record {}", transaction_id.to_string(), flow->getUUIDStr());
      auto end_time = std::chrono::steady_clock::now();
      std::string transit_uri = peer_->getURL() + "/" + flow->getUUIDStr();
      std::string details = "urn:nifi:" + flow->getUUIDStr() + "Remote Host=" + peer_->getHostName();
      session.getProvenanceReporter()->send(*flow, transit_uri, details, std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time), false);
      session.remove(flow);

      std::chrono::nanoseconds transfer_duration = std::chrono::high_resolution_clock::now() - transaction_started_at;
      if (transfer_duration > batch_send_nanos_) {
        break;
      }

      flow = session.get();
      if (!flow) {
        break;
      }
    }

    if (!confirm(transaction_id)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed for " + transaction_id.to_string());
    }
    if (!complete(context, transaction_id)) {
      throw Exception(SITE2SITE_EXCEPTION, "Complete Failed for " + transaction_id.to_string());
    }
    logger_->log_debug("Site2Site transaction {} successfully sent flow record {}, content bytes {}", transaction_id.to_string(), transaction->getCurrentTransfers(), transaction->getBytes());
  } catch (const std::exception& exception) {
    handleTransactionError(transaction, context, exception);
    throw;
  }

  deleteTransaction(transaction_id);
  return true;
}

bool SiteToSiteClient::confirmReceive(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id) {
  if (transaction->isDataAvailable()) {
    return false;
  }
  // we received a FINISH_TRANSACTION indicator. Send back a CONFIRM_TRANSACTION message
  // to peer so that we can verify that the connection is still open. This is a two-phase commit,
  // which helps to prevent the chances of data duplication. Without doing this, we may commit the
  // session and then when we send the response back to the peer, the peer may have timed out and may not
  // be listening. As a result, it will re-send the data. By doing this two-phase commit, we narrow the
  // Critical Section involved in this transaction so that rather than the Critical Section being the
  // time window involved in the entire transaction, it is reduced to a simple round-trip conversation.
  uint64_t crcValue = transaction->getCRC();
  std::string crc = std::to_string(crcValue);
  logger_->log_debug("Site2Site Receive confirm with CRC {} to transaction {}", crcValue, transaction_id.to_string());
  if (!writeResponse(transaction, {ResponseCode::CONFIRM_TRANSACTION, crc})) {
    return false;
  }

  auto response = readResponse(transaction);
  if (!response) {
    return false;
  }

  if (response->code == ResponseCode::CONFIRM_TRANSACTION) {
    logger_->log_debug("Site2Site transaction {} peer confirm transaction", transaction_id.to_string());
    transaction->setState(TransactionState::TRANSACTION_CONFIRMED);
    return true;
  } else if (response->code == ResponseCode::BAD_CHECKSUM) {
    logger_->log_debug("Site2Site transaction {} peer indicate bad checksum", transaction_id.to_string());
    return false;
  }

  logger_->log_debug("Site2Site transaction {} peer unknown response code {}", transaction_id.to_string(), magic_enum::enum_underlying(response->code));
  return false;
}

bool SiteToSiteClient::confirmSend(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id) {
  logger_->log_debug("Site2Site Send FINISH TRANSACTION for transaction {}", transaction_id.to_string());
  if (!writeResponse(transaction, {ResponseCode::FINISH_TRANSACTION, "FINISH_TRANSACTION"})) {
    return false;
  }

  auto response = readResponse(transaction);
  if (!response) {
    return false;
  }

  if (response->code != ResponseCode::CONFIRM_TRANSACTION) {
    logger_->log_debug("Site2Site transaction {} peer unknown response code {}", transaction_id.to_string(), magic_enum::enum_underlying(response->code));
    return false;
  }

  // we've sent a FINISH_TRANSACTION. Now we'll wait for the peer to send a 'Confirm Transaction' response
  logger_->log_debug("Site2Site transaction {} peer confirm transaction with CRC {}", transaction_id.to_string(), response->message);
  if (current_version_ > 3) {
    std::string crc = std::to_string(transaction->getCRC());
    if (response->message == crc) {
      logger_->log_debug("Site2Site transaction {} CRC matched", transaction_id.to_string());
      if (!writeResponse(transaction, {ResponseCode::CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION"})) {
        return false;
      }
      transaction->setState(TransactionState::TRANSACTION_CONFIRMED);
      return true;
    } else {
      logger_->log_debug("Site2Site transaction {} CRC not matched {}", transaction_id.to_string(), crc);
      writeResponse(transaction, {ResponseCode::BAD_CHECKSUM, "BAD_CHECKSUM"});
      return false;
    }
  } else {
    if (!writeResponse(transaction, {ResponseCode::CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION"})) {
      return false;
    }
    transaction->setState(TransactionState::TRANSACTION_CONFIRMED);
    return true;
  }
}

bool SiteToSiteClient::confirm(const utils::Identifier& transaction_id) {
  if (peer_state_ != PeerState::READY) {
    bootstrap();
  }

  if (peer_state_ != PeerState::READY) {
    return false;
  }

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    return false;
  }

  auto transaction = it->second;
  if (transaction->getState() == TransactionState::TRANSACTION_STARTED && !transaction->isDataAvailable() &&
      transaction->getDirection() == TransferDirection::RECEIVE) {
    transaction->setState(TransactionState::TRANSACTION_CONFIRMED);
    return true;
  }

  if (transaction->getState() != TransactionState::DATA_EXCHANGED) {
    return false;
  }

  if (transaction->getDirection() == TransferDirection::RECEIVE) {
    return confirmReceive(transaction, transaction_id);
  }

  return confirmSend(transaction, transaction_id);
}

void SiteToSiteClient::cancel(const utils::Identifier& transaction_id) {
  if (peer_state_ != PeerState::READY) {
    return;
  }

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    logger_->log_warn("Site2Site transaction id '{}' not found for cancel", transaction_id.to_string());
    return;
  }

  auto transaction = it->second;
  if (transaction->getState() == TransactionState::TRANSACTION_CANCELED || transaction->getState() == TransactionState::TRANSACTION_COMPLETED ||
      transaction->getState() == TransactionState::TRANSACTION_ERROR) {
    logger_->log_debug("Site2Site transaction {} already canceled or completed or in error state", transaction_id.to_string());
    return;
  }

  writeResponse(transaction, {ResponseCode::CANCEL_TRANSACTION, "Cancel"});
  transaction->setState(TransactionState::TRANSACTION_CANCELED);

  tearDown();
}

void SiteToSiteClient::error(const utils::Identifier& transaction_id) {
  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    logger_->log_warn("Site2Site transaction id '{}' not found for error", transaction_id.to_string());
    return;
  }

  auto transaction = it->second;
  transaction->setState(TransactionState::TRANSACTION_ERROR);
  tearDown();
}

bool SiteToSiteClient::completeReceive(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id) {
  if (transaction->getCurrentTransfers() == 0) {
    transaction->setState(TransactionState::TRANSACTION_COMPLETED);
    return true;
  }

  logger_->log_debug("Site2Site transaction {} receive finished", transaction_id.to_string());
  if (!writeResponse(transaction, {ResponseCode::TRANSACTION_FINISHED, "Finished"})) {
    return false;
  }

  transaction->setState(TransactionState::TRANSACTION_COMPLETED);
  return true;
}

bool SiteToSiteClient::completeSend(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id, core::ProcessContext& context) {
  auto response = readResponse(transaction);
  if (!response) {
    return false;
  }

  if (response->code == ResponseCode::TRANSACTION_FINISHED || response->code == ResponseCode::TRANSACTION_FINISHED_BUT_DESTINATION_FULL) {
    logger_->log_info("Site2Site transaction {} peer finished transaction", transaction_id.to_string());
    transaction->setState(TransactionState::TRANSACTION_COMPLETED);

    if (response->code == ResponseCode::TRANSACTION_FINISHED_BUT_DESTINATION_FULL) {
      logger_->log_info("Site2Site transaction {} reported destination full, yielding", transaction_id.to_string());
      context.yield();
    }
    return true;
  }

  logger_->log_warn("Site2Site transaction {} peer unexpected response code {}: {}", transaction_id.to_string(), magic_enum::enum_underlying(response->code), magic_enum::enum_name(response->code));
  return false;
}

bool SiteToSiteClient::complete(core::ProcessContext& context, const utils::Identifier& transaction_id) {
  if (peer_state_ != PeerState::READY) {
    bootstrap();
  }

  if (peer_state_ != PeerState::READY) {
    return false;
  }

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    logger_->log_warn("Site2Site transaction id '{}' not found for complete", transaction_id.to_string());
    return false;
  }

  auto transaction = it->second;
  if (transaction->getTotalTransfers() > 0 && transaction->getState() != TransactionState::TRANSACTION_CONFIRMED) {
    logger_->log_warn("Site2Site transaction {} not confirmed", transaction_id.to_string());
    return false;
  }

  if (transaction->getDirection() == TransferDirection::RECEIVE) {
    return completeReceive(transaction, transaction_id);
  }

  return completeSend(transaction, transaction_id, context);
}

bool SiteToSiteClient::send(const utils::Identifier& transaction_id, DataPacket* packet, const std::shared_ptr<core::FlowFile> &flow_file, core::ProcessSession* session) {
  if (peer_state_ != PeerState::READY) {
    bootstrap();
  }

  if (peer_state_ != PeerState::READY) {
    return false;
  }

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    return false;
  }

  auto transaction = it->second;
  if (transaction->getState() != TransactionState::TRANSACTION_STARTED && transaction->getState() != TransactionState::DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction {} is not at started or exchanged state", transaction_id.to_string());
    return false;
  }

  if (transaction->getDirection() != TransferDirection::SEND) {
    logger_->log_warn("Site2Site transaction {} direction is wrong", transaction_id.to_string());
    return false;
  }

  if (transaction->getCurrentTransfers() > 0) {
    if (!writeResponse(transaction, {ResponseCode::CONTINUE_TRANSACTION, "CONTINUE_TRANSACTION"})) {
      return false;
    }
  }
  // start to read the packet
  if (const auto ret = transaction->getStream().write(gsl::narrow<uint32_t>(packet->attributes.size())); ret != 4) {
    logger_->log_error("Failed to write number of attributes!");
    return false;
  }

  for (const auto& attribute : packet->attributes) {
    if (const auto ret = transaction->getStream().write(attribute.first, true); ret == 0 || io::isError(ret)) {
      logger_->log_error("Failed to write attribute key {}!", attribute.first);
      return false;
    }
    if (const auto ret = transaction->getStream().write(attribute.second, true); ret == 0 || io::isError(ret)) {
      logger_->log_error("Failed to write attribute value {}!", attribute.second);
      return false;
    }
    logger_->log_debug("Site2Site transaction {} send attribute key {} value {}", transaction_id.to_string(), attribute.first, attribute.second);
  }

  bool flowfile_has_content = [&]() {
    if (!flow_file) {
      return false;
    }
    if (flow_file->getResourceClaim() == nullptr || !flow_file->getResourceClaim()->exists()) {
      auto path = flow_file->getResourceClaim() != nullptr ? flow_file->getResourceClaim()->getContentFullPath() : "nullclaim";
      logger_->log_debug("Claim {} does not exist for FlowFile {}", path, flow_file->getUUIDStr());
      return false;
    }
    return true;
  }();

  uint64_t len = 0;
  if (flow_file && flowfile_has_content && session) {
    len = flow_file->getSize();
    const auto ret = transaction->getStream().write(len);
    if (ret != 8) {
      logger_->log_debug("Failed to write content size!");
      return false;
    }
    if (flow_file->getSize() > 0) {
      session->read(flow_file, [packet](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
        const auto result = internal::pipe(*input_stream, packet->transaction->getStream());
        if (result == -1) return false;
        packet->size = gsl::narrow<size_t>(result);
        return true;
      });
      if (flow_file->getSize() != packet->size) {
        logger_->log_debug("Mismatched sizes {} {}", flow_file->getSize(), packet->size);
        return false;
      }
    }
    if (packet->payload.length() == 0 && len == 0) {
      if (flow_file->getResourceClaim() == nullptr) {
        logger_->log_trace("no claim");
      } else {
        logger_->log_trace("Flowfile empty {}", flow_file->getResourceClaim()->getContentFullPath());
      }
    }
  } else if (packet->payload.length() > 0) {
    len = packet->payload.length();
    if (const auto ret = transaction->getStream().write(len); ret != 8) {
      logger_->log_debug("Failed to write payload size!");
      return false;
    }
    if (const auto ret = transaction->getStream().write(reinterpret_cast<const uint8_t*>(packet->payload.c_str()), gsl::narrow<size_t>(len)); ret != gsl::narrow<size_t>(len)) {
      logger_->log_debug("Failed to write payload!");
      return false;
    }
    packet->size += len;
  } else if (flow_file && !flowfile_has_content) {
    const auto ret = transaction->getStream().write(len);  // Indicate zero length
    if (ret != 8) {
      logger_->log_debug("Failed to write content size (0)!");
      return false;
    }
  }

  transaction->incrementCurrentTransfers();
  transaction->incrementTotalTransfers();
  transaction->setState(TransactionState::DATA_EXCHANGED);
  transaction->addBytes(len);

  logger_->log_info("Site to Site transaction {} sent flow {} flow records, with total size {}", transaction_id.to_string(), transaction->getTotalTransfers(), transaction->getBytes());

  return true;
}

bool SiteToSiteClient::receive(const utils::Identifier& transaction_id, DataPacket *packet, bool &eof) {
  if (peer_state_ != PeerState::READY) {
    bootstrap();
  }

  if (peer_state_ != PeerState::READY) {
    return false;
  }

  auto it = known_transactions_.find(transaction_id);
  if (it == known_transactions_.end()) {
    return false;
  }

  auto transaction = it->second;
  if (transaction->getState() != TransactionState::TRANSACTION_STARTED && transaction->getState() != TransactionState::DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction {} is not at started or exchanged state", transaction_id.to_string());
    return false;
  }

  if (transaction->getDirection() != TransferDirection::RECEIVE) {
    logger_->log_warn("Site2Site transaction {} direction is wrong", transaction_id.to_string());
    return false;
  }

  if (!transaction->isDataAvailable()) {
    eof = true;
    return true;
  }

  if (transaction->getCurrentTransfers() > 0) {
    // if we already has transfer before, check to see whether another one is available
    auto response = readResponse(transaction);
    if (!response) {
      return false;
    }
    if (response->code == ResponseCode::CONTINUE_TRANSACTION) {
      logger_->log_debug("Site2Site transaction {} peer indicate continue transaction", transaction_id.to_string());
      transaction->setDataAvailable(true);
    } else if (response->code == ResponseCode::FINISH_TRANSACTION) {
      logger_->log_debug("Site2Site transaction {} peer indicate finish transaction", transaction_id.to_string());
      transaction->setDataAvailable(false);
      eof = true;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction {} peer indicate wrong response code {}", transaction_id.to_string(), magic_enum::enum_underlying(response->code));
      return false;
    }
  }

  if (!transaction->isDataAvailable()) {
    logger_->log_debug("No data is available");
    eof = true;
    return true;
  }

  // start to read the packet
  uint32_t num_attributes = 0;
  if (const auto ret = transaction->getStream().read(num_attributes); ret == 0 || io::isError(ret) || num_attributes > MAX_NUM_ATTRIBUTES) {
    return false;
  }

  // read the attributes
  logger_->log_debug("Site2Site transaction {} receives {} attributes", transaction_id.to_string(), num_attributes);
  for (uint64_t i = 0; i < num_attributes; i++) {
    std::string key;
    std::string value;
    if (const auto ret = transaction->getStream().read(key, true); ret == 0 || io::isError(ret)) {
      return false;
    }

    if (const auto ret = transaction->getStream().read(value, true); ret == 0 || io::isError(ret)) {
      return false;
    }

    packet->attributes[key] = value;
    logger_->log_debug("Site2Site transaction {} receives attribute key {} value {}", transaction_id.to_string(), key, value);
  }

  uint64_t len = 0;
  if (const auto ret = transaction->getStream().read(len); ret == 0 || io::isError(ret)) {
    return false;
  }

  packet->size = len;
  if (len > 0 || num_attributes > 0) {
    transaction->incrementCurrentTransfers();
    transaction->incrementTotalTransfers();
  } else {
    logger_->log_warn("Site2Site transaction {} empty flow file without attribute", transaction_id.to_string());
    transaction->setDataAvailable(false);
    eof = true;
    return true;
  }
  transaction->setState(TransactionState::DATA_EXCHANGED);
  transaction->addBytes(len);

  logger_->log_info("Site to Site transaction {} received flow record {}, total length {}, added {}",
      transaction_id.to_string(), transaction->getTotalTransfers(), transaction->getBytes(), len);

  return true;
}

std::pair<uint64_t, uint64_t> SiteToSiteClient::readFlowFiles(const std::shared_ptr<Transaction>& transaction, core::ProcessSession& session) {
  uint64_t transfers = 0;
  uint64_t bytes = 0;
  utils::Identifier transaction_id = transaction->getUUID();
  while (true) {
    auto start_time = std::chrono::steady_clock::now();
    std::string payload;
    DataPacket packet(transaction, payload);
    bool eof = false;

    if (!receive(transaction_id, &packet, eof)) {
      throw Exception(SITE2SITE_EXCEPTION, "Receive Failed " + transaction_id.to_string());
    }
    if (eof) {
      // transaction done
      break;
    }

    auto flow_file = session.create();
    if (!flow_file) {
      throw Exception(SITE2SITE_EXCEPTION, "Flow File Creation Failed");
    }

    std::string source_identifier;
    for (const auto& [key, value] : packet.attributes) {
      if (key == core::SpecialFlowAttribute::UUID) {
        source_identifier = value;
      }
      flow_file->addAttribute(key, value);
    }

    if (packet.size > 0) {
      session.write(flow_file, [&packet](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
        uint64_t len = packet.size;
        std::array<std::byte, utils::configuration::DEFAULT_BUFFER_SIZE> buffer{};
        while (len > 0) {
          const auto size = std::min(len, uint64_t{utils::configuration::DEFAULT_BUFFER_SIZE});
          const auto ret = packet.transaction->getStream().read(std::as_writable_bytes(std::span(buffer).subspan(0, size)));
          if (ret != size) {
            return false;
          }
          output_stream->write(std::span(buffer).subspan(0, size));
          len -= size;
        }
        return true;
      });
      if (flow_file->getSize() != packet.size) {
        std::stringstream message;
        message << "Receive size not correct, expected to send " << flow_file->getSize() << " bytes, but actually sent " << packet.size;
        throw Exception(SITE2SITE_EXCEPTION, message.str());
      } else {
        logger_->log_debug("received {} with expected {}", flow_file->getSize(), packet.size);
      }
    }
    core::Relationship relation{"undefined", ""};
    auto end_time = std::chrono::steady_clock::now();
    std::string transitUri = peer_->getURL() + "/" + source_identifier;
    std::string details = "urn:nifi:" + source_identifier + "Remote Host=" + peer_->getHostName();
    session.getProvenanceReporter()->receive(*flow_file, transitUri, source_identifier, details, std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
    session.transfer(flow_file, relation);
    // receive the transfer for the flow record
    bytes += packet.size;
    transfers++;
  }

  return {transfers, bytes};
}

bool SiteToSiteClient::receiveFlowFiles(core::ProcessContext& context, core::ProcessSession& session) {
  if (peer_state_ != PeerState::READY) {
    if (!bootstrap()) {
      return false;
    }
  }

  if (peer_state_ != PeerState::READY) {
    context.yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
  }

  auto transaction = createTransaction(TransferDirection::RECEIVE);
  if (transaction == nullptr) {
    context.yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  utils::Identifier transaction_id = transaction->getUUID();
  try {
    auto [transfers, bytes] = readFlowFiles(transaction, session);

    if (transfers > 0 && !confirm(transaction_id)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Transaction Failed");
    }

    if (!complete(context, transaction_id)) {
      std::stringstream transaction_str;
      transaction_str << "Complete Transaction " << transaction_id.to_string() << " Failed";
      throw Exception(SITE2SITE_EXCEPTION, transaction_str.str());
    }

    logger_->log_info("Site to Site transaction {} received flow record {}, with content size {} bytes", transaction_id.to_string(), transfers, bytes);
    // we yield the receive if we did not get anything
    if (transfers == 0) {
      context.yield();
    }
  } catch (const std::exception& exception) {
    handleTransactionError(transaction, context, exception);
    throw;
  }

  deleteTransaction(transaction_id);
  return true;
}

const ResponseCodeContext* SiteToSiteClient::getResponseCodeContext(ResponseCode code) {
  auto it = std::find_if(respond_code_contexts.begin(), respond_code_contexts.end(), [code](const ResponseCodeContext& context) {
    return context.code == code;
  });
  if (it != respond_code_contexts.end()) {
    return &(*it);
  }
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::sitetosite
