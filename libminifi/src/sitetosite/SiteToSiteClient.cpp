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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

int SiteToSiteClient::writeRequestType(RequestType type) {
  if (type >= MAX_REQUEST_TYPE)
    return -1;

  return peer_->writeUTF(SiteToSiteRequest::RequestTypeStr[type]);
}

int SiteToSiteClient::readRequestType(RequestType &type) {
  std::string requestTypeStr;

  int ret = peer_->readUTF(requestTypeStr);

  if (ret <= 0)
    return ret;

  for (int i = NEGOTIATE_FLOWFILE_CODEC; i <= SHUTDOWN; i++) {
    if (SiteToSiteRequest::RequestTypeStr[i] == requestTypeStr) {
      type = (RequestType) i;
      return ret;
    }
  }

  return -1;
}

int SiteToSiteClient::readResponse(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message) {
  uint8_t firstByte;

  int ret = peer_->read(firstByte);

  if (ret <= 0 || firstByte != CODE_SEQUENCE_VALUE_1)
    return -1;

  uint8_t secondByte;

  ret = peer_->read(secondByte);

  if (ret <= 0 || secondByte != CODE_SEQUENCE_VALUE_2)
    return -1;

  uint8_t thirdByte;

  ret = peer_->read(thirdByte);

  if (ret <= 0)
    return ret;

  code = (RespondCode) thirdByte;

  RespondCodeContext *resCode = this->getRespondCodeContext(code);

  if (resCode == NULL) {
    // Not a valid respond code
    return -1;
  }
  if (resCode->hasDescription) {
    ret = peer_->readUTF(message);
    if (ret <= 0)
      return -1;
  }
  return 3 + message.size();
}

void SiteToSiteClient::deleteTransaction(std::string transactionID) {
  std::shared_ptr<Transaction> transaction = NULL;

  std::map<std::string, std::shared_ptr<Transaction> >::iterator it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  } else {
    transaction = it->second;
  }

  logger_->log_debug("Site2Site delete transaction %s", transaction->getUUIDStr());
  known_transactions_.erase(transactionID);
}

int SiteToSiteClient::writeResponse(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message) {
  RespondCodeContext *resCode = this->getRespondCodeContext(code);

  if (resCode == NULL) {
    // Not a valid respond code
    return -1;
  }

  uint8_t codeSeq[3];
  codeSeq[0] = CODE_SEQUENCE_VALUE_1;
  codeSeq[1] = CODE_SEQUENCE_VALUE_2;
  codeSeq[2] = (uint8_t) code;

  int ret = peer_->write(codeSeq, 3);

  if (ret != 3)
    return -1;

  if (resCode->hasDescription) {
    ret = peer_->writeUTF(message);
    if (ret > 0) {
      return (3 + ret);
    } else {
      return ret;
    }
  } else {
    return 3;
  }
}

void SiteToSiteClient::tearDown() {
  if (peer_state_ >= ESTABLISHED) {
    logger_->log_debug("Site2Site Protocol tearDown");
    // need to write shutdown request
    writeRequestType(SHUTDOWN);
  }

  known_transactions_.clear();
  peer_->Close();
  peer_state_ = IDLE;
}

bool SiteToSiteClient::transferFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow = std::static_pointer_cast<FlowFileRecord>(session->get());

  std::shared_ptr<Transaction> transaction = NULL;

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
  std::string transactionID;
  transaction = createTransaction(transactionID, SEND);
  if (transaction == NULL) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  bool continueTransaction = true;
  uint64_t startSendingNanos = getTimeNano();

  try {
    while (continueTransaction) {
      uint64_t startTime = getTimeMillis();
      std::string payload;
      DataPacket packet(getLogger(), transaction, flow->getAttributes(), payload);

      int16_t resp = send(transactionID, &packet, flow, session);
      if (resp == -1) {
        throw Exception(SITE2SITE_EXCEPTION, "Send Failed");
      }

      logger_->log_debug("Site2Site transaction %s send flow record %s", transactionID, flow->getUUIDStr());
      if (resp == 0) {
        uint64_t endTime = getTimeMillis();
        std::string transitUri = peer_->getURL() + "/" + flow->getUUIDStr();
        std::string details = "urn:nifi:" + flow->getUUIDStr() + "Remote Host=" + peer_->getHostName();
        session->getProvenanceReporter()->send(flow, transitUri, details, endTime - startTime, false);
      }
      session->remove(flow);

      uint64_t transferNanos = getTimeNano() - startSendingNanos;
      if (transferNanos > _batchSendNanos)
        break;

      flow = std::static_pointer_cast<FlowFileRecord>(session->get());

      if (!flow) {
        continueTransaction = false;
      }
    }  // while true

    if (!confirm(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed for " + transactionID);
    }
    if (!complete(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Complete Failed for " + transactionID);
    }
    logger_->log_debug("Site2Site transaction %s successfully send flow record %d, content bytes %llu", transactionID, transaction->total_transfers_, transaction->_bytes);
  } catch (std::exception &exception) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_debug("Caught Exception during SiteToSiteClient::transferFlowFiles");
    throw;
  }

  deleteTransaction(transactionID);

  return true;
}

bool SiteToSiteClient::confirm(std::string transactionID) {
  int ret;
  std::shared_ptr<Transaction> transaction = NULL;

  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return false;
  }

  std::map<std::string, std::shared_ptr<Transaction> >::iterator it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return false;
  } else {
    transaction = it->second;
  }

  if (transaction->getState() == TRANSACTION_STARTED && !transaction->isDataAvailable() && transaction->getDirection() == RECEIVE) {
    transaction->_state = TRANSACTION_CONFIRMED;
    return true;
  }

  if (transaction->getState() != DATA_EXCHANGED)
    return false;

  if (transaction->getDirection() == RECEIVE) {
    if (transaction->isDataAvailable())
      return false;
    // we received a FINISH_TRANSACTION indicator. Send back a CONFIRM_TRANSACTION message
    // to peer so that we can verify that the connection is still open. This is a two-phase commit,
    // which helps to prevent the chances of data duplication. Without doing this, we may commit the
    // session and then when we send the response back to the peer, the peer may have timed out and may not
    // be listening. As a result, it will re-send the data. By doing this two-phase commit, we narrow the
    // Critical Section involved in this transaction so that rather than the Critical Section being the
    // time window involved in the entire transaction, it is reduced to a simple round-trip conversation.
    int64_t crcValue = transaction->getCRC();
    std::string crc = std::to_string(crcValue);
    logger_->log_debug("Site2Site Send confirm with CRC %d to transaction %s", transaction->getCRC(), transactionID);
    ret = writeResponse(transaction, CONFIRM_TRANSACTION, crc);
    if (ret <= 0)
      return false;
    RespondCode code;
    std::string message;
    readResponse(transaction, code, message);
    if (ret <= 0)
      return false;

    if (code == CONFIRM_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer confirm transaction", transactionID);
      transaction->_state = TRANSACTION_CONFIRMED;
      return true;
    } else if (code == BAD_CHECKSUM) {
      logger_->log_debug("Site2Site transaction %s peer indicate bad checksum", transactionID);
      return false;
    } else {
      logger_->log_debug("Site2Site transaction %s peer unknown response code %d", transactionID, code);
      return false;
    }
  } else {
    logger_->log_debug("Site2Site Send FINISH TRANSACTION for transaction %s", transactionID);
    ret = writeResponse(transaction, FINISH_TRANSACTION, "FINISH_TRANSACTION");
    if (ret <= 0)
      return false;
    RespondCode code;
    std::string message;
    readResponse(transaction, code, message);

    // we've sent a FINISH_TRANSACTION. Now we'll wait for the peer to send a 'Confirm Transaction' response
    if (code == CONFIRM_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer confirm transaction with CRC %s", transactionID, message);
      if (this->_currentVersion > 3) {
        int64_t crcValue = transaction->getCRC();
        std::string crc = std::to_string(crcValue);
        if (message == crc) {
          logger_->log_debug("Site2Site transaction %s CRC matched", transactionID);
          ret = writeResponse(transaction, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
          if (ret <= 0)
            return false;
          transaction->_state = TRANSACTION_CONFIRMED;
          return true;
        } else {
          logger_->log_debug("Site2Site transaction %s CRC not matched %s", transactionID, crc);
          ret = writeResponse(transaction, BAD_CHECKSUM, "BAD_CHECKSUM");
          return false;
        }
      }
      ret = writeResponse(transaction, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
      if (ret <= 0)
        return false;
      transaction->_state = TRANSACTION_CONFIRMED;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction %s peer unknown respond code %d", transactionID, code);
      return false;
    }
    return false;
  }
}

void SiteToSiteClient::cancel(std::string transactionID) {
  std::shared_ptr<Transaction> transaction = NULL;

  if (peer_state_ != READY) {
    return;
  }

  std::map<std::string, std::shared_ptr<Transaction> >::iterator it = this->known_transactions_.find(transactionID);

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
  return;
}

void SiteToSiteClient::error(std::string transactionID) {
  std::shared_ptr<Transaction> transaction = NULL;

  std::map<std::string, std::shared_ptr<Transaction> >::iterator it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return;
  } else {
    transaction = it->second;
  }

  transaction->_state = TRANSACTION_ERROR;
  tearDown();
  return;
}

// Complete the transaction
bool SiteToSiteClient::complete(std::string transactionID) {
  int ret;
  std::shared_ptr<Transaction> transaction = NULL;

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
      logger_->log_debug("Site2Site transaction %s send finished", transactionID);
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
    int ret;

    ret = readResponse(transaction, code, message);

    if (ret <= 0)
      return false;

    if (code == TRANSACTION_FINISHED) {
      logger_->log_info("Site2Site transaction %s peer finished transaction", transactionID);
      transaction->_state = TRANSACTION_COMPLETED;
      return true;
    } else {
      logger_->log_warn("Site2Site transaction %s peer unknown respond code %d", transactionID, code);
      return false;
    }
  }
}

int16_t SiteToSiteClient::send(std::string transactionID, DataPacket *packet, const std::shared_ptr<FlowFileRecord> &flowFile, const std::shared_ptr<core::ProcessSession> &session) {
  int ret;
  std::shared_ptr<Transaction> transaction = NULL;

  if (flowFile && (flowFile->getResourceClaim() == nullptr || !flowFile->getResourceClaim()->exists())) {
    auto path = flowFile->getResourceClaim() != nullptr ? flowFile->getResourceClaim()->getContentFullPath() : "nullclaim";
    logger_->log_debug("Claim %s does not exist for FlowFile %s", path, flowFile->getUUIDStr());
    return -2;
  }
  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return -1;
  }
  std::map<std::string, std::shared_ptr<Transaction> >::iterator it = this->known_transactions_.find(transactionID);

  if (it == known_transactions_.end()) {
    return -1;
  } else {
    transaction = it->second;
  }

  if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction %s is not at started or exchanged state", transactionID);
    return -1;
  }

  if (transaction->getDirection() != SEND) {
    logger_->log_warn("Site2Site transaction %s direction is wrong", transactionID);
    return -1;
  }

  if (transaction->current_transfers_ > 0) {
    ret = writeResponse(transaction, CONTINUE_TRANSACTION, "CONTINUE_TRANSACTION");
    if (ret <= 0) {
      return -1;
    }
  }
  // start to read the packet
  uint32_t numAttributes = packet->_attributes.size();
  ret = transaction->getStream().write(numAttributes);
  if (ret != 4) {
    return -1;
  }

  std::map<std::string, std::string>::iterator itAttribute;
  for (itAttribute = packet->_attributes.begin(); itAttribute != packet->_attributes.end(); itAttribute++) {
    ret = transaction->getStream().writeUTF(itAttribute->first, true);

    if (ret <= 0) {
      return -1;
    }
    ret = transaction->getStream().writeUTF(itAttribute->second, true);
    if (ret <= 0) {
      return -1;
    }
    logger_->log_debug("Site2Site transaction %s send attribute key %s value %s", transactionID, itAttribute->first, itAttribute->second);
  }

  uint64_t len = 0;
  if (flowFile) {
    len = flowFile->getSize();
    ret = transaction->getStream().write(len);
    if (ret != 8) {
      logger_->log_debug("ret != 8");
      return -1;
    }
    if (flowFile->getSize() > 0) {
      sitetosite::ReadCallback callback(packet);
      session->read(flowFile, &callback);
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

    ret = transaction->getStream().write(len);
    if (ret != 8) {
      return -1;
    }

    ret = transaction->getStream().writeData(reinterpret_cast<uint8_t *>(const_cast<char*>(packet->payload_.c_str())), len);
    if (ret != (int64_t)len) {
      logger_->log_debug("ret != len");
      return -1;
    }
    packet->_size += len;
  }

  transaction->current_transfers_++;
  transaction->total_transfers_++;
  transaction->_state = DATA_EXCHANGED;
  transaction->_bytes += len;

  logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID << " sent flow " << transaction->total_transfers_
                             << "flow records, with total size " << transaction->_bytes;

  return 0;
}

bool SiteToSiteClient::receive(std::string transactionID, DataPacket *packet, bool &eof) {
  int ret;
  std::shared_ptr<Transaction> transaction = NULL;

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

  if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED) {
    logger_->log_warn("Site2Site transaction %s is not at started or exchanged state", transactionID);
    return false;
  }

  if (transaction->getDirection() != RECEIVE) {
    logger_->log_warn("Site2Site transaction %s direction is wrong", transactionID);
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

    ret = readResponse(transaction, code, message);

    if (ret <= 0) {
      return false;
    }
    if (code == CONTINUE_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer indicate continue transaction", transactionID);
      transaction->_dataAvailable = true;
    } else if (code == FINISH_TRANSACTION) {
      logger_->log_debug("Site2Site transaction %s peer indicate finish transaction", transactionID);
      transaction->_dataAvailable = false;
      eof = true;
      return true;
    } else {
      logger_->log_debug("Site2Site transaction %s peer indicate wrong respond code %d", transactionID, code);
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
  ret = transaction->getStream().read(numAttributes);
  if (ret <= 0 || numAttributes > MAX_NUM_ATTRIBUTES) {
    return false;
  }

  // read the attributes
  logger_->log_debug("Site2Site transaction %s receives attribute key %d", transactionID, numAttributes);
  for (unsigned int i = 0; i < numAttributes; i++) {
    std::string key;
    std::string value;
    ret = transaction->getStream().readUTF(key, true);
    if (ret <= 0) {
      return false;
    }
    ret = transaction->getStream().readUTF(value, true);
    if (ret <= 0) {
      return false;
    }
    packet->_attributes[key] = value;
    logger_->log_debug("Site2Site transaction %s receives attribute key %s value %s", transactionID, key, value);
  }

  uint64_t len;
  ret = transaction->getStream().read(len);
  if (ret <= 0) {
    return false;
  }

  packet->_size = len;
  if (len > 0) {
    transaction->current_transfers_++;
    transaction->total_transfers_++;
  } else {
    logger_->log_debug("Site2Site transaction %s receives attribute ?", transactionID);
    transaction->_dataAvailable = false;
    eof = true;
    return true;
  }
  transaction->_state = DATA_EXCHANGED;
  transaction->_bytes += len;
  logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID << " received flow record " << transaction->total_transfers_
                             << ", total length " << transaction->_bytes << ", added " << len;

  return true;
}

bool SiteToSiteClient::receiveFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  uint64_t bytes = 0;
  int transfers = 0;
  std::shared_ptr<Transaction> transaction = NULL;

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
  std::string transactionID;
  transaction = createTransaction(transactionID, RECEIVE);

  if (transaction == NULL) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  try {
    while (true) {
      std::map<std::string, std::string> empty;
      uint64_t startTime = getTimeMillis();
      std::string payload;
      DataPacket packet(getLogger(), transaction, empty, payload);
      bool eof = false;

      if (!receive(transactionID, &packet, eof)) {
        throw Exception(SITE2SITE_EXCEPTION, "Receive Failed");
      }
      if (eof) {
        // transaction done
        break;
      }
      std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());

      if (!flowFile) {
        throw Exception(SITE2SITE_EXCEPTION, "Flow File Creation Failed");
      }
      std::map<std::string, std::string>::iterator it;
      std::string sourceIdentifier;
      for (it = packet._attributes.begin(); it != packet._attributes.end(); it++) {
        if (it->first == FlowAttributeKey(UUID))
          sourceIdentifier = it->second;
        flowFile->addAttribute(it->first, it->second);
      }

      if (packet._size > 0) {
        sitetosite::WriteCallback callback(&packet);
        session->write(flowFile, &callback);
        if (flowFile->getSize() != packet._size) {
          std::stringstream message;
          message << "Receive size not correct, expected to send " << flowFile->getSize() << " bytes, but actually sent " << packet._size;
          throw Exception(SITE2SITE_EXCEPTION, message.str());
        } else {
          logger_->log_debug("received %llu with expected %llu", flowFile->getSize(), packet._size);
        }
      }
      core::Relationship relation;  // undefined relationship
      uint64_t endTime = getTimeMillis();
      std::string transitUri = peer_->getURL() + "/" + sourceIdentifier;
      std::string details = "urn:nifi:" + sourceIdentifier + "Remote Host=" + peer_->getHostName();
      session->getProvenanceReporter()->receive(flowFile, transitUri, sourceIdentifier, details, endTime - startTime);
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
      transaction_str << "Complete Transaction " << transactionID << " Failed";
      throw Exception(SITE2SITE_EXCEPTION, transaction_str.str());
    }
    logging::LOG_INFO(logger_) << "Site to Site transaction " << transactionID << " received flow record " << transfers
                               << ", with content size " << bytes << " bytes";

    // we yield the receive if we did not get anything
    if (transfers == 0)
      context->yield();
  } catch (std::exception &exception) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_warn("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    if (transaction)
      deleteTransaction(transactionID);
    context->yield();
    tearDown();
    logger_->log_warn("Caught Exception during RawSiteToSiteClient::receiveFlowFiles");
    throw;
  }

  deleteTransaction(transactionID);

  return true;
}
} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
