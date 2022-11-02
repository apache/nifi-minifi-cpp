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

namespace org::apache::nifi::minifi::sitetosite {

int SiteToSiteClient::readResponse(const std::shared_ptr<Transaction>& /*transaction*/, RespondCode &code, std::string &message) {
  uint8_t firstByte;
  {
    const auto ret = peer_->read(firstByte);
    if (ret == 0 || io::isError(ret) || firstByte != CODE_SEQUENCE_VALUE_1)
      return -1;
  }

  uint8_t secondByte;
  {
    const auto ret = peer_->read(secondByte);
    if (ret == 0 || io::isError(ret) || secondByte != CODE_SEQUENCE_VALUE_2)
      return -1;
  }

  uint8_t thirdByte;
  {
    const auto ret = peer_->read(thirdByte);
    if (ret == 0 || io::isError(ret))
      return static_cast<int>(ret);
  }

  code = (RespondCode) thirdByte;
  RespondCodeContext *resCode = this->getRespondCodeContext(code);
  if (!resCode) {
    return -1;
  }
  if (resCode->hasDescription) {
    const auto ret = peer_->read(message);
    if (ret == 0 || io::isError(ret))
      return -1;
  }
  return gsl::narrow<int>(3 + message.size());
}

void SiteToSiteClient::deleteTransaction(const utils::Identifier& transactionID) {
  std::shared_ptr<Transaction> transaction;

  auto it = this->known_transactions_.find(transactionID);
  if (it == known_transactions_.end()) {
    return;
  } else {
    transaction = it->second;
  }

  logger_->log_debug("Site2Site delete transaction %s", transaction->getUUIDStr());
  known_transactions_.erase(transactionID);
}

int SiteToSiteClient::writeResponse(const std::shared_ptr<Transaction>& /*transaction*/, RespondCode code, const std::string& message) {
  RespondCodeContext *resCode = this->getRespondCodeContext(code);
  if (!resCode) {
    return -1;
  }

  {
    const uint8_t codeSeq[3] { CODE_SEQUENCE_VALUE_1, CODE_SEQUENCE_VALUE_2, static_cast<uint8_t>(code) };
    const auto ret = peer_->write(codeSeq, 3);
    if (ret != 3)
      return -1;
  }

  if (resCode->hasDescription) {
    const auto ret = peer_->write(message);
    if (io::isError(ret)) return -1;
    if (ret == 0) return 0;
    return 3 + gsl::narrow<int>(ret);
  } else {
    return 3;
  }
}

bool SiteToSiteClient::transferFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto flow = session->get();

  std::shared_ptr<Transaction> transaction = nullptr;

  if (!flow) {
    return false;
  }

  if (peer_state_ != READY) {
    if (!bootstrap())
      return false;
  }

  if (peer_state_ != READY) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
  }

  // Create the transaction
  transaction = createTransaction(SEND);
  if (transaction == nullptr) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }
  utils::Identifier transactionID = transaction->getUUID();

  bool continueTransaction = true;
  std::chrono::high_resolution_clock::time_point transaction_started_at = std::chrono::high_resolution_clock::now();

  try {
    while (continueTransaction) {
      auto start_time = std::chrono::steady_clock::now();
      std::string payload;
      DataPacket packet(getLogger(), transaction, flow->getAttributes(), payload);

      int16_t resp = send(transactionID, &packet, flow, session);
      if (resp == -1) {
        throw Exception(SITE2SITE_EXCEPTION, "Send Failed");
      }

      logger_->log_debug("Site2Site transaction %s send flow record %s", transactionID.to_string(), flow->getUUIDStr());
      if (resp == 0) {
        auto end_time = std::chrono::steady_clock::now();
        std::string transitUri = peer_->getURL() + "/" + flow->getUUIDStr();
        std::string details = "urn:nifi:" + flow->getUUIDStr() + "Remote Host=" + peer_->getHostName();
        session->getProvenanceReporter()->send(flow, transitUri, details, std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time), false);
      }
      session->remove(flow);

      std::chrono::nanoseconds transfer_duration = std::chrono::high_resolution_clock::now() - transaction_started_at;
      if (transfer_duration > _batchSendNanos)
        break;

      flow = session->get();

      if (!flow) {
        continueTransaction = false;
      }
    }  // while true

    if (!confirm(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed for " + transactionID.to_string());
    }
    if (!complete(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Complete Failed for " + transactionID.to_string());
    }
    logger_->log_debug("Site2Site transaction %s successfully sent flow record %d, content bytes %llu", transactionID.to_string(), transaction->total_transfers_, transaction->_bytes);
  } catch (std::exception &exception) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_debug("Caught Exception during SiteToSiteClient::transferFlowFiles, type: %s, what: %s", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_debug("Caught Exception during SiteToSiteClient::transferFlowFiles, type: %s", getCurrentExceptionTypeName());
    throw;
  }

  deleteTransaction(transactionID);

  return true;
}

bool SiteToSiteClient::confirm(const utils::Identifier& transactionID) {
  int ret;
  std::shared_ptr<Transaction> transaction;

  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return false;
  }

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return false;
  }
  transaction = it->second;


  if (transaction->getState() == TRANSACTION_STARTED && !transaction->isDataAvailable() &&
      transaction->getDirection() == RECEIVE) {
    transaction->_state = TRANSACTION_CONFIRMED;
    return true;
  }

  if (transaction->getState() != DATA_EXCHANGED) {
    return false;
  }

  if (transaction->getDirection() == RECEIVE) {
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
    logger_->log_debug("Site2Site Receive confirm with CRC %llu to transaction %s", crcValue, transactionID.to_string());
    ret = writeResponse(transaction, CONFIRM_TRANSACTION, crc);
    if (ret <= 0)
      return false;
    RespondCode code;
    std::string message;
    readResponse(transaction, code, message);
    if (ret <= 0)
      return false;

    if (code == CONFIRM_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer confirm transaction", transactionID.to_string());
      transaction->_state = TRANSACTION_CONFIRMED;
      return true;
    } else if (code == BAD_CHECKSUM) {
      logger_->log_debug("Site2Site transaction %s peer indicate bad checksum", transactionID.to_string());
      return false;
    } else {
      logger_->log_debug("Site2Site transaction %s peer unknown response code %d", transactionID.to_string(), code);
      return false;
    }
  } else {
    logger_->log_debug("Site2Site Send FINISH TRANSACTION for transaction %s", transactionID.to_string());
    ret = writeResponse(transaction, FINISH_TRANSACTION, "FINISH_TRANSACTION");
    if (ret <= 0) {
      return false;
    }
    RespondCode code;
    std::string message;
    readResponse(transaction, code, message);

    // we've sent a FINISH_TRANSACTION. Now we'll wait for the peer to send a 'Confirm Transaction' response
    if (code == CONFIRM_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer confirm transaction with CRC %s", transactionID.to_string(), message);
      if (this->_currentVersion > 3) {
        uint64_t crcValue = transaction->getCRC();
        std::string crc = std::to_string(crcValue);
        if (message == crc) {
          logger_->log_debug("Site2Site transaction %s CRC matched", transactionID.to_string());
          ret = writeResponse(transaction, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
          if (ret <= 0)
            return false;
          transaction->_state = TRANSACTION_CONFIRMED;
          return true;
        } else {
          logger_->log_debug("Site2Site transaction %s CRC not matched %s", transactionID.to_string(), crc);
          writeResponse(transaction, BAD_CHECKSUM, "BAD_CHECKSUM");
          return false;
        }
      }
      ret = writeResponse(transaction, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
      if (ret <= 0)
        return false;
      transaction->_state = TRANSACTION_CONFIRMED;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction %s peer unknown respond code %d", transactionID.to_string(), code);
      return false;
    }
    return false;
  }
}

void SiteToSiteClient::cancel(const utils::Identifier& transactionID) {
  std::shared_ptr<Transaction> transaction = nullptr;

  if (peer_state_ != READY) {
    return;
  }

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  } else {
    transaction = it->second;
  }

  if (transaction->getState() == TRANSACTION_CANCELED || transaction->getState() == TRANSACTION_COMPLETED || transaction->getState() == TRANSACTION_ERROR) {
    return;
  }

  this->writeResponse(transaction, CANCEL_TRANSACTION, "Cancel");
  transaction->_state = TRANSACTION_CANCELED;

  tearDown();
}

void SiteToSiteClient::error(const utils::Identifier& transactionID) {
  std::shared_ptr<Transaction> transaction = nullptr;

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  } else {
    transaction = it->second;
  }

  transaction->_state = TRANSACTION_ERROR;
  tearDown();
}

// Complete the transaction
bool SiteToSiteClient::complete(const utils::Identifier& transactionID) {
  int ret;
  std::shared_ptr<Transaction> transaction = nullptr;

  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return false;
  }

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return false;
  } else {
    transaction = it->second;
  }

  if (transaction->total_transfers_ > 0 && transaction->getState() != TRANSACTION_CONFIRMED) {
    return false;
  }
  if (transaction->getDirection() == RECEIVE) {
    if (transaction->current_transfers_ == 0) {
      transaction->_state = TRANSACTION_COMPLETED;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction %s receive finished", transactionID.to_string());
      ret = this->writeResponse(transaction, TRANSACTION_FINISHED, "Finished");
      if (ret <= 0) {
        return false;
      } else {
        transaction->_state = TRANSACTION_COMPLETED;
        return true;
      }
    }
  } else {
    RespondCode code;
    std::string message;

    ret = readResponse(transaction, code, message);

    if (ret <= 0)
      return false;

    if (code == TRANSACTION_FINISHED) {
      logger_->log_info("Site2Site transaction %s peer finished transaction", transactionID.to_string());
      transaction->_state = TRANSACTION_COMPLETED;
      return true;
    } else {
      logger_->log_warn("Site2Site transaction %s peer unknown respond code %d", transactionID.to_string(), code);
      return false;
    }
  }
}

int16_t SiteToSiteClient::send(const utils::Identifier &transactionID, DataPacket *packet, const std::shared_ptr<core::FlowFile> &flowFile, const std::shared_ptr<core::ProcessSession> &session) {
  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return -1;
  }

  auto it = this->known_transactions_.find(transactionID);
  if (it == known_transactions_.end()) {
    return -1;
  }
  std::shared_ptr<Transaction> transaction = it->second;

  if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction %s is not at started or exchanged state", transactionID.to_string());
    return -1;
  }

  if (transaction->getDirection() != SEND) {
    logger_->log_warn("Site2Site transaction %s direction is wrong", transactionID.to_string());
    return -1;
  }

  if (transaction->current_transfers_ > 0) {
    const auto ret = writeResponse(transaction, CONTINUE_TRANSACTION, "CONTINUE_TRANSACTION");
    if (ret <= 0) {
      return -1;
    }
  }
  // start to read the packet
  {
    const auto numAttributes = gsl::narrow<uint32_t>(packet->_attributes.size());
    const auto ret = transaction->getStream().write(numAttributes);
    if (ret != 4) {
      return -1;
    }
  }

  for (const auto& attribute : packet->_attributes) {
    {
      const auto ret = transaction->getStream().write(attribute.first, true);
      if (ret == 0 || io::isError(ret)) {
        return -1;
      }
    }
    {
      const auto ret = transaction->getStream().write(attribute.second, true);
      if (ret == 0 || io::isError(ret)) {
        return -1;
      }
    }
    logger_->log_debug("Site2Site transaction %s send attribute key %s value %s", transactionID.to_string(), attribute.first, attribute.second);
  }

  bool flowfile_has_content = (flowFile != nullptr);

  if (flowFile && (flowFile->getResourceClaim() == nullptr || !flowFile->getResourceClaim()->exists())) {
    auto path = flowFile->getResourceClaim() != nullptr ? flowFile->getResourceClaim()->getContentFullPath() : "nullclaim";
    logger_->log_debug("Claim %s does not exist for FlowFile %s", path, flowFile->getUUIDStr());
    flowfile_has_content = false;
  }

  uint64_t len = 0;
  if (flowFile && flowfile_has_content) {
    len = flowFile->getSize();
    const auto ret = transaction->getStream().write(len);
    if (ret != 8) {
      logger_->log_debug("Failed to write content size!");
      return -1;
    }
    if (flowFile->getSize() > 0) {
      session->read(flowFile, [packet](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
        const auto result = internal::pipe(*input_stream, packet->transaction_->getStream());
        if (result == -1) return -1;
        packet->_size = gsl::narrow<size_t>(result);
        return result;
      });
      if (flowFile->getSize() != packet->_size) {
        logger_->log_debug("Mismatched sizes %llu %llu", flowFile->getSize(), packet->_size);
        return -2;
      }
    }
    if (packet->payload_.length() == 0 && len == 0) {
      if (flowFile->getResourceClaim() == nullptr)
        logger_->log_trace("no claim");
      else
        logger_->log_trace("Flowfile empty %s", flowFile->getResourceClaim()->getContentFullPath());
    }
  } else if (packet->payload_.length() > 0) {
    len = packet->payload_.length();
    {
      const auto ret = transaction->getStream().write(len);
      if (ret != 8) {
        return -1;
      }
    }
    {
      const auto ret = transaction->getStream().write(reinterpret_cast<const uint8_t*>(packet->payload_.c_str()), gsl::narrow<size_t>(len));
      if (ret != gsl::narrow<size_t>(len)) {
        logger_->log_debug("Failed to write payload size!");
        return -1;
      }
    }
    packet->_size += len;
  } else if (flowFile && !flowfile_has_content) {
    const auto ret = transaction->getStream().write(len);  // Indicate zero length
    if (ret != 8) {
      logger_->log_debug("Failed to write content size (0)!");
      return -1;
    }
  }

  transaction->current_transfers_++;
  transaction->total_transfers_++;
  transaction->_state = DATA_EXCHANGED;
  transaction->_bytes += len;

  core::logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID.to_string() << " sent flow " << transaction->total_transfers_
                             << "flow records, with total size " << transaction->_bytes;

  return 0;
}

bool SiteToSiteClient::receive(const utils::Identifier& transactionID, DataPacket *packet, bool &eof) {
  std::shared_ptr<Transaction> transaction;

  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return false;
  }

  auto it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return false;
  }

  transaction = it->second;

  if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction %s is not at started or exchanged state", transactionID.to_string());
    return false;
  }

  if (transaction->getDirection() != RECEIVE) {
    logger_->log_warn("Site2Site transaction %s direction is wrong", transactionID.to_string());
    return false;
  }

  if (!transaction->isDataAvailable()) {
    eof = true;
    return true;
  }

  if (transaction->current_transfers_ > 0) {
    // if we already has transfer before, check to see whether another one is available
    RespondCode code;
    std::string message;

    if (readResponse(transaction, code, message) <= 0) {
      return false;
    }
    if (code == CONTINUE_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer indicate continue transaction", transactionID.to_string());
      transaction->_dataAvailable = true;
    } else if (code == FINISH_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer indicate finish transaction", transactionID.to_string());
      transaction->_dataAvailable = false;
      eof = true;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction %s peer indicate wrong respond code %d", transactionID.to_string(), code);
      return false;
    }
  }

  if (!transaction->isDataAvailable()) {
    logger_->log_debug("No data is available");
    eof = true;
    return true;
  }

  // start to read the packet
  uint32_t numAttributes;
  {
    const auto ret = transaction->getStream().read(numAttributes);
    if (ret == 0 || io::isError(ret) || numAttributes > MAX_NUM_ATTRIBUTES) {
      return false;
    }
  }

  // read the attributes
  logger_->log_debug("Site2Site transaction %s receives attribute key %d", transactionID.to_string(), numAttributes);
  for (unsigned int i = 0; i < numAttributes; i++) {
    std::string key;
    std::string value;
    {
      const auto ret = transaction->getStream().read(key, true);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = transaction->getStream().read(value, true);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    packet->_attributes[key] = value;
    logger_->log_debug("Site2Site transaction %s receives attribute key %s value %s", transactionID.to_string(), key, value);
  }

  uint64_t len;
  {
    const auto ret = transaction->getStream().read(len);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  packet->_size = len;
  if (len > 0 || numAttributes > 0) {
    transaction->current_transfers_++;
    transaction->total_transfers_++;
  } else {
    logger_->log_warn("Site2Site transaction %s empty flow file without attribute", transactionID.to_string());
    transaction->_dataAvailable = false;
    eof = true;
    return true;
  }
  transaction->_state = DATA_EXCHANGED;
  transaction->_bytes += len;
  core::logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID.to_string() << " received flow record " << transaction->total_transfers_
                             << ", total length " << transaction->_bytes << ", added " << len;

  return true;
}

bool SiteToSiteClient::receiveFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  uint64_t bytes = 0;
  int transfers = 0;
  std::shared_ptr<Transaction> transaction = nullptr;

  if (peer_state_ != READY) {
    if (!bootstrap()) {
      return false;
    }
  }

  if (peer_state_ != READY) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
  }

  // Create the transaction
  transaction = createTransaction(RECEIVE);

  if (transaction == nullptr) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  utils::Identifier transactionID = transaction->getUUID();

  try {
    while (true) {
      std::map<std::string, std::string> empty;
      auto start_time = std::chrono::steady_clock::now();
      std::string payload;
      DataPacket packet(getLogger(), transaction, empty, payload);
      bool eof = false;

      if (!receive(transactionID, &packet, eof)) {
        throw Exception(SITE2SITE_EXCEPTION, "Receive Failed " + transactionID.to_string());
      }
      if (eof) {
        // transaction done
        break;
      }
      auto flowFile = session->create();

      if (!flowFile) {
        throw Exception(SITE2SITE_EXCEPTION, "Flow File Creation Failed");
      }
      std::map<std::string, std::string>::iterator it;
      std::string sourceIdentifier;
      for (it = packet._attributes.begin(); it != packet._attributes.end(); it++) {
        if (it->first == core::SpecialFlowAttribute::UUID)
          sourceIdentifier = it->second;
        flowFile->addAttribute(it->first, it->second);
      }

      if (packet._size > 0) {
        session->write(flowFile, [&packet](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
          return internal::pipe(packet.transaction_->getStream(), *output_stream);
        });
        if (flowFile->getSize() != packet._size) {
          std::stringstream message;
          message << "Receive size not correct, expected to send " << flowFile->getSize() << " bytes, but actually sent " << packet._size;
          throw Exception(SITE2SITE_EXCEPTION, message.str());
        } else {
          logger_->log_debug("received %llu with expected %llu", flowFile->getSize(), packet._size);
        }
      }
      core::Relationship relation;  // undefined relationship
      auto end_time = std::chrono::steady_clock::now();
      std::string transitUri = peer_->getURL() + "/" + sourceIdentifier;
      std::string details = "urn:nifi:" + sourceIdentifier + "Remote Host=" + peer_->getHostName();
      session->getProvenanceReporter()->receive(flowFile, transitUri, sourceIdentifier, details, std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
      session->transfer(flowFile, relation);
      // receive the transfer for the flow record
      bytes += packet._size;
      transfers++;
    }  // while true

    if (transfers > 0 && !confirm(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Transaction Failed");
    }
    if (!complete(transactionID)) {
      std::stringstream transaction_str;
      transaction_str << "Complete Transaction " << transactionID.to_string() << " Failed";
      throw Exception(SITE2SITE_EXCEPTION, transaction_str.str());
    }
    core::logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID.to_string() << " received flow record " << transfers
                               << ", with content size " << bytes << " bytes";
    // we yield the receive if we did not get anything
    if (transfers == 0)
      context->yield();
  } catch (std::exception &exception) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_warn("Caught Exception during RawSiteToSiteClient::receiveFlowFiles, type: %s, what: %s", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_warn("Caught Exception during RawSiteToSiteClient::receiveFlowFiles, type: %s", getCurrentExceptionTypeName());
    throw;
  }

  deleteTransaction(transactionID);
  return true;
}
}  // namespace org::apache::nifi::minifi::sitetosite
