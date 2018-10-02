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
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <utility>
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <random>
#include <iostream>
#include <vector>

#include "sitetosite/RawSocketProtocol.h"
#include "io/CRCStream.h"
#include "sitetosite/Peer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

std::shared_ptr<utils::IdGenerator> RawSiteToSiteClient::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<utils::IdGenerator> Transaction::id_generator_ = utils::IdGenerator::getIdGenerator();

const char *RawSiteToSiteClient::HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY] = {
/**
 * Boolean value indicating whether or not the contents of a FlowFile should
 * be GZipped when transferred.
 */
"GZIP",
/**
 * The unique identifier of the port to communicate with
 */
"PORT_IDENTIFIER",
/**
 * Indicates the number of milliseconds after the request was made that the
 * client will wait for a response. If no response has been received by the
 * time this value expires, the server can move on without attempting to
 * service the request because the client will have already disconnected.
 */
"REQUEST_EXPIRATION_MILLIS",
/**
 * The preferred number of FlowFiles that the server should send to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol.
 */
"BATCH_COUNT",
/**
 * The preferred number of bytes that the server should send to the client
 * when pulling data. This property was introduced in version 5 of the
 * protocol.
 */
"BATCH_SIZE",
/**
 * The preferred amount of time that the server should send data to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol. Value is in milliseconds.
 */
"BATCH_DURATION" };

bool RawSiteToSiteClient::establish() {
  if (peer_state_ != IDLE) {
    logger_->log_error("Site2Site peer state is not idle while try to establish");
    return false;
  }

  bool ret = peer_->Open();

  if (!ret) {
    logger_->log_error("Site2Site peer socket open failed");
    return false;
  }

  // Negotiate the version
  ret = initiateResourceNegotiation();

  if (!ret) {
    logger_->log_error("Site2Site Protocol Version Negotiation failed");
    return false;
  }

  logger_->log_debug("Site2Site socket established");
  peer_state_ = ESTABLISHED;

  return true;
}

bool RawSiteToSiteClient::initiateResourceNegotiation() {
  // Negotiate the version
  if (peer_state_ != IDLE) {
    logger_->log_error("Site2Site peer state is not idle while initiateResourceNegotiation");
    return false;
  }

  logger_->log_debug("Negotiate protocol version with destination port %s current version %d", port_id_str_, _currentVersion);

  int ret = peer_->writeUTF(getResourceName());

  logger_->log_trace("result of writing resource name is %i", ret);
  if (ret <= 0) {
    logger_->log_debug("result of writing resource name is %i", ret);
    // tearDown();
    return false;
  }

  ret = peer_->write(_currentVersion);

  if (ret <= 0) {
    logger_->log_debug("result of writing version is %i", ret);
    return false;
  }

  uint8_t statusCode;
  ret = peer_->read(statusCode);

  if (ret <= 0) {
    logger_->log_debug("result of writing version status code  %i", ret);
    return false;
  }
  logger_->log_debug("status code is %i", statusCode);
  switch (statusCode) {
    case RESOURCE_OK:
      logger_->log_debug("Site2Site Protocol Negotiate protocol version OK");
      return true;
    case DIFFERENT_RESOURCE_VERSION:
      uint32_t serverVersion;
      ret = peer_->read(serverVersion);
      if (ret <= 0) {
        return false;
      }

      logging::LOG_INFO(logger_) << "Site2Site Server Response asked for a different protocol version " << serverVersion;

      for (unsigned int i = (_currentVersionIndex + 1); i < sizeof(_supportedVersion) / sizeof(uint32_t); i++) {
        if (serverVersion >= _supportedVersion[i]) {
          _currentVersion = _supportedVersion[i];
          _currentVersionIndex = i;
          return initiateResourceNegotiation();
        }
      }
      ret = -1;
      return false;
    case NEGOTIATED_ABORT:
      logger_->log_warn("Site2Site Negotiate protocol response ABORT");
      ret = -1;
      return false;
    default:
      logger_->log_warn("Negotiate protocol response unknown code %d", statusCode);
      return true;
  }

  return true;
}

bool RawSiteToSiteClient::initiateCodecResourceNegotiation() {
  // Negotiate the version
  if (peer_state_ != HANDSHAKED) {
    logger_->log_error("Site2Site peer state is not handshaked while initiateCodecResourceNegotiation");
    return false;
  }

  logger_->log_trace("Negotiate Codec version with destination port %s current version %d", port_id_str_, _currentCodecVersion);

  int ret = peer_->writeUTF(getCodecResourceName());

  if (ret <= 0) {
    logger_->log_debug("result of getCodecResourceName is %i", ret);
    return false;
  }

  ret = peer_->write(_currentCodecVersion);

  if (ret <= 0) {
    logger_->log_debug("result of _currentCodecVersion is %i", ret);
    return false;
  }

  uint8_t statusCode;
  ret = peer_->read(statusCode);

  if (ret <= 0) {
    return false;
  }

  switch (statusCode) {
    case RESOURCE_OK:
      logger_->log_trace("Site2Site Codec Negotiate version OK");
      return true;
    case DIFFERENT_RESOURCE_VERSION:
      uint32_t serverVersion;
      ret = peer_->read(serverVersion);
      if (ret <= 0) {
        return false;
      }
      logging::LOG_INFO(logger_) << "Site2Site Server Response asked for a different protocol version " << serverVersion;

      for (unsigned int i = (_currentCodecVersionIndex + 1); i < sizeof(_supportedCodecVersion) / sizeof(uint32_t); i++) {
        if (serverVersion >= _supportedCodecVersion[i]) {
          _currentCodecVersion = _supportedCodecVersion[i];
          _currentCodecVersionIndex = i;
          return initiateCodecResourceNegotiation();
        }
      }
      ret = -1;
      return false;
    case NEGOTIATED_ABORT:
      logger_->log_error("Site2Site Codec Negotiate response ABORT");
      ret = -1;
      return false;
    default:
      logger_->log_error("Negotiate Codec response unknown code %d", statusCode);
      return true;
  }

  return true;
}

bool RawSiteToSiteClient::handShake() {
  if (peer_state_ != ESTABLISHED) {
    logger_->log_error("Site2Site peer state is not established while handshake");
    return false;
  }
  logger_->log_debug("Site2Site Protocol Perform hand shake with destination port %s", port_id_str_);
  utils::Identifier uuid;
  // Generate the global UUID for the com identify
  id_generator_->generate(uuid);
  _commsIdentifier = uuid.to_string();

  int ret = peer_->writeUTF(_commsIdentifier);

  if (ret <= 0) {
    return false;
  }

  std::map<std::string, std::string> properties;
  properties[HandShakePropertyStr[GZIP]] = "false";
  properties[HandShakePropertyStr[PORT_IDENTIFIER]] = port_id_str_;
  properties[HandShakePropertyStr[REQUEST_EXPIRATION_MILLIS]] = std::to_string(_timeOut);
  if (_currentVersion >= 5) {
    if (_batchCount > 0)
      properties[HandShakePropertyStr[BATCH_COUNT]] = std::to_string(_batchCount);
    if (_batchSize > 0)
      properties[HandShakePropertyStr[BATCH_SIZE]] = std::to_string(_batchSize);
    if (_batchDuration > 0)
      properties[HandShakePropertyStr[BATCH_DURATION]] = std::to_string(_batchDuration);
  }

  if (_currentVersion >= 3) {
    ret = peer_->writeUTF(peer_->getURL());
    if (ret <= 0) {
      return false;
    }
  }

  uint32_t size = properties.size();
  ret = peer_->write(size);
  if (ret <= 0) {
    return false;
  }

  std::map<std::string, std::string>::iterator it;
  for (it = properties.begin(); it != properties.end(); it++) {
    ret = peer_->writeUTF(it->first);
    if (ret <= 0) {
      return false;
    }
    ret = peer_->writeUTF(it->second);
    if (ret <= 0) {
      return false;
    }
    logger_->log_debug("Site2Site Protocol Send handshake properties %s %s", it->first, it->second);
  }

  RespondCode code;
  std::string message;

  ret = readRespond(nullptr, code, message);

  if (ret <= 0) {
    return false;
  }

  std::string error;

  switch (code) {
    case PROPERTIES_OK:
      logger_->log_debug("Site2Site HandShake Completed");
      peer_state_ = HANDSHAKED;
      return true;
    case PORT_NOT_IN_VALID_STATE:
      error = "in invalid state";
      break;
    case UNKNOWN_PORT:
      error = "an unknown port";
      break;
    case PORTS_DESTINATION_FULL:
      error = "full";
      break;
    // Unknown error
    default:
      logger_->log_error("HandShake Failed because of unknown respond code %d", code);
      ret = -1;
      return false;
  }

  // All known error cases handled here
  logger_->log_error("Site2Site HandShake Failed because destination port, %s, is %s", port_id_str_, error);
  ret = -1;
  return false;
}

void RawSiteToSiteClient::tearDown() {
  if (peer_state_ >= ESTABLISHED) {
    logger_->log_trace("Site2Site Protocol tearDown");
    // need to write shutdown request
    writeRequestType(SHUTDOWN);
  }

  known_transactions_.clear();
  peer_->Close();
  peer_state_ = IDLE;
}

bool RawSiteToSiteClient::getPeerList(std::vector<PeerStatus> &peers) {
  if (establish() && handShake()) {
    int status = writeRequestType(REQUEST_PEER_LIST);

    if (status <= 0) {
      tearDown();
      return false;
    }

    uint32_t number;
    status = peer_->read(number);

    if (status <= 0) {
      tearDown();
      return false;
    }

    for (uint32_t i = 0; i < number; i++) {
      std::string host;
      status = peer_->readUTF(host);
      if (status <= 0) {
        tearDown();
        return false;
      }
      uint32_t port;
      status = peer_->read(port);
      if (status <= 0) {
        tearDown();
        return false;
      }
      uint8_t secure;
      status = peer_->read(secure);
      if (status <= 0) {
        tearDown();
        return false;
      }
      uint32_t count;
      status = peer_->read(count);
      if (status <= 0) {
        tearDown();
        return false;
      }
      PeerStatus status(std::make_shared<Peer>(port_id_, host, port, secure), count, true);
      peers.push_back(std::move(status));
      logging::LOG_TRACE(logger_) << "Site2Site Peer host " << host << " port " << port << " Secure " << secure;
    }

    tearDown();
    return true;
  } else {
    tearDown();
    return false;
  }
}

int RawSiteToSiteClient::writeRequestType(RequestType type) {
  if (type >= MAX_REQUEST_TYPE)
    return -1;

  return peer_->writeUTF(SiteToSiteRequest::RequestTypeStr[type]);
}

int RawSiteToSiteClient::readRequestType(RequestType &type) {
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

int RawSiteToSiteClient::readRespond(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message) {
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

  RespondCodeContext *resCode = getRespondCodeContext(code);

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

int RawSiteToSiteClient::writeRespond(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message) {
  RespondCodeContext *resCode = getRespondCodeContext(code);

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

bool RawSiteToSiteClient::negotiateCodec() {
  if (peer_state_ != HANDSHAKED) {
    logger_->log_error("Site2Site peer state is not handshaked while negotiate codec");
    return false;
  }

  logger_->log_trace("Site2Site Protocol Negotiate Codec with destination port %s", port_id_str_);

  int status = writeRequestType(NEGOTIATE_FLOWFILE_CODEC);

  if (status <= 0) {
    return false;
  }

  // Negotiate the codec version
  bool ret = initiateCodecResourceNegotiation();

  if (!ret) {
    logger_->log_error("Site2Site Codec Version Negotiation failed");
    return false;
  }

  logger_->log_trace("Site2Site Codec Completed and move to READY state for data transfer");
  peer_state_ = READY;

  return true;
}

bool RawSiteToSiteClient::bootstrap() {
  if (peer_state_ == READY)
    return true;

  tearDown();

  if (establish() && handShake() && negotiateCodec()) {
    logger_->log_debug("Site to Site ready for data transaction");
    return true;
  } else {
    peer_->yield();
    tearDown();
    return false;
  }
}

std::shared_ptr<Transaction> RawSiteToSiteClient::createTransaction(std::string &transactionID, TransferDirection direction) {
  int ret;
  bool dataAvailable;
  std::shared_ptr<Transaction> transaction = nullptr;

  if (peer_state_ != READY) {
    bootstrap();
  }

  if (peer_state_ != READY) {
    return transaction;
  }

  if (direction == RECEIVE) {
    ret = writeRequestType(RECEIVE_FLOWFILES);

    if (ret <= 0) {
      return transaction;
    }

    RespondCode code;
    std::string message;

    ret = readRespond(nullptr, code, message);

    if (ret <= 0) {
      return transaction;
    }

    org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(peer_.get());
    switch (code) {
      case MORE_DATA:
        dataAvailable = true;
        logger_->log_trace("Site2Site peer indicates that data is available");
        transaction = std::make_shared<Transaction>(direction, crcstream);
        known_transactions_[transaction->getUUIDStr()] = transaction;
        transactionID = transaction->getUUIDStr();
        transaction->setDataAvailable(dataAvailable);
        logger_->log_trace("Site2Site create transaction %s", transaction->getUUIDStr());
        return transaction;
      case NO_MORE_DATA:
        dataAvailable = false;
        logger_->log_trace("Site2Site peer indicates that no data is available");
        transaction = std::make_shared<Transaction>(direction, crcstream);
        known_transactions_[transaction->getUUIDStr()] = transaction;
        transactionID = transaction->getUUIDStr();
        transaction->setDataAvailable(dataAvailable);
        logger_->log_trace("Site2Site create transaction %s", transaction->getUUIDStr());
        return transaction;
      default:
        logger_->log_warn("Site2Site got unexpected response %d when asking for data", code);
        return NULL;
    }
  } else {
    ret = writeRequestType(SEND_FLOWFILES);

    if (ret <= 0) {
      return NULL;
    } else {
      org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcstream(peer_.get());
      transaction = std::make_shared<Transaction>(direction, crcstream);
      known_transactions_[transaction->getUUIDStr()] = transaction;
      transactionID = transaction->getUUIDStr();
      logger_->log_trace("Site2Site create transaction %s", transaction->getUUIDStr());
      return transaction;
    }
  }
}

bool RawSiteToSiteClient::transmitPayload(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session, const std::string &payload,
                                          std::map<std::string, std::string> attributes) {
  std::shared_ptr<Transaction> transaction = NULL;

  if (payload.length() <= 0)
    return false;

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
  transaction = createTransaction(transactionID, SEND);

  if (transaction == NULL) {
    context->yield();
    tearDown();
    throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
  }

  try {
    DataPacket packet(getLogger(), transaction, attributes, payload);

    int16_t resp = send(transactionID, &packet, nullptr, session);
    if (resp == -1) {
      throw Exception(SITE2SITE_EXCEPTION, "Send Failed");
    }
    logging::LOG_INFO(logger_) << "Site2Site transaction " << transactionID << " sent bytes length" << payload.length();

    if (!confirm(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed");
    }
    if (!complete(transactionID)) {
      throw Exception(SITE2SITE_EXCEPTION, "Complete Failed");
    }
    logging::LOG_INFO(logger_) << "Site2Site transaction " << transactionID << " successfully send flow record " << transaction->current_transfers_ << " content bytes " << transaction->_bytes;
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
    logger_->log_debug("Caught Exception during RawSiteToSiteClient::transferBytes");
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
