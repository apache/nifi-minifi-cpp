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

#ifndef LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITECLIENT_H_
#define LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITECLIENT_H_

#include "Peer.h"
#include "SiteToSite.h"
#include "core/ProcessSession.h"
#include "core/ProcessContext.h"
#include "core/Connectable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

/**
 * Represents a piece of data that is to be sent to or that was received from a
 * NiFi instance.
 */
class DataPacket {
 public:
  DataPacket(const std::shared_ptr<logging::Logger> &logger, const std::shared_ptr<Transaction> &transaction, std::map<std::string, std::string> attributes, const std::string &payload)
      : payload_(payload),
        logger_reference_(logger) {
    _size = 0;
    transaction_ = transaction;
    _attributes = attributes;
  }
  std::map<std::string, std::string> _attributes;
  uint64_t _size;
  std::shared_ptr<Transaction> transaction_;
  const std::string & payload_;
  std::shared_ptr<logging::Logger> logger_reference_;
};

class SiteToSiteClient : public core::Connectable {

 public:

  SiteToSiteClient()
      : core::Connectable("SitetoSiteClient"),
        peer_state_(IDLE),
        _batchSendNanos(5000000000),
        ssl_context_service_(nullptr),
        logger_(logging::LoggerFactory<SiteToSiteClient>::getLogger()) {
    _supportedVersion[0] = 5;
    _supportedVersion[1] = 4;
    _supportedVersion[2] = 3;
    _supportedVersion[3] = 2;
    _supportedVersion[4] = 1;
    _currentVersion = _supportedVersion[0];
    _currentVersionIndex = 0;
    _supportedCodecVersion[0] = 1;
    _currentCodecVersion = _supportedCodecVersion[0];
    _currentCodecVersionIndex = 0;
  }

  virtual ~SiteToSiteClient() {

  }

  void setSSLContextService(const std::shared_ptr<minifi::controllers::SSLContextService> &context_service) {
    ssl_context_service_ = context_service;
  }

  /**
   * Creates a transaction using the transaction ID and the direction
   * @param transactionID transaction identifier
   * @param direction direction of transfer
   */
  virtual std::shared_ptr<Transaction> createTransaction(std::string &transactionID, TransferDirection direction) = 0;

  /**
   * Transfers flow files
   * @param direction transfer direction
   * @param context process context
   * @param session process session
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */
  virtual bool transfer(TransferDirection direction, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
#ifndef WIN32
	  if (__builtin_expect(direction == SEND, 1)) {
      return transferFlowFiles(context, session);
    } else {
      return receiveFlowFiles(context, session);
    }
#else
	  if (direction == SEND) {
		  return transferFlowFiles(context, session);
	  }
	  else {
		  return receiveFlowFiles(context, session);
	  }
#endif
  }

  /**
   * Transfers flow files to server
   * @param context process context
   * @param session process session
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */
  virtual bool transferFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  /**
   * Receive flow files from server
   * @param context process context
   * @param session process session
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */

  // Confirm the data that was sent or received by comparing CRC32's of the data sent and the data received.
  // Receive flow files for the process session
  bool receiveFlowFiles(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  // Receive the data packet from the transaction
  // Return false when any error occurs
  bool receive(std::string transactionID, DataPacket *packet, bool &eof);
  /**
   * Transfers raw data and attributes  to server
   * @param context process context
   * @param session process session
   * @param payload data to transmit
   * @param attributes
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */
  virtual bool transmitPayload(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session, const std::string &payload,
                               std::map<std::string, std::string> attributes) = 0;

  void setPortId(utils::Identifier &id) {
    port_id_ = id;
    port_id_str_ = port_id_.to_string();
  }

  /**
   * Sets the base peer for this interface.
   */
  virtual void setPeer(std::unique_ptr<SiteToSitePeer> peer) {
    peer_ = std::move(peer);
  }

  /**
   * Provides a reference to the port identifier
   * @returns port identifier
   */
  const std::string getPortId() const {
    return port_id_str_;
  }

  /**
   * Obtains the peer list and places them into the provided vector
   * @param peers peer vector.
   * @return true if successful, false otherwise
   */
  virtual bool getPeerList(std::vector<PeerStatus> &peers) = 0;

  /**
   * Establishes the interface.
   * @return true if successful, false otherwise
   */
  virtual bool establish() = 0;

  const std::shared_ptr<logging::Logger> &getLogger() {
    return logger_;
  }

  virtual void yield() {

  }

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() {
    return running_;
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() {
    return true;
  }

  virtual bool bootstrap() {
    return true;
  }

  // Return -1 when any error occurs
  virtual int16_t send(std::string transactionID, DataPacket *packet, const std::shared_ptr<FlowFileRecord> &flowFile, const std::shared_ptr<core::ProcessSession> &session);

 protected:

  // Cancel the transaction
  virtual void cancel(std::string transactionID);
  // Complete the transaction
  virtual bool complete(std::string transactionID);
  // Error the transaction
  virtual void error(std::string transactionID);

  virtual bool confirm(std::string transactionID);
  // deleteTransaction
  virtual void deleteTransaction(std::string transactionID);

  virtual void tearDown();

  // write Request Type
  virtual int writeRequestType(RequestType type);
  // read Request Type
  virtual int readRequestType(RequestType &type);
  // read Respond
  virtual int readResponse(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message);
  // write respond
  virtual int writeResponse(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message);
  // getRespondCodeContext
  virtual RespondCodeContext *getRespondCodeContext(RespondCode code) {
    for (unsigned int i = 0; i < sizeof(SiteToSiteRequest::respondCodeContext) / sizeof(RespondCodeContext); i++) {
      if (SiteToSiteRequest::respondCodeContext[i].code == code) {
        return &SiteToSiteRequest::respondCodeContext[i];
      }
    }
    return NULL;
  }

  // Peer State
  PeerState peer_state_;

  // portIDStr
  std::string port_id_str_;

  // portId
  utils::Identifier port_id_;

  // Peer Connection
  std::unique_ptr<SiteToSitePeer> peer_;

  std::atomic<bool> running_;

  // transaction map
  std::map<std::string, std::shared_ptr<Transaction>> known_transactions_;

  // BATCH_SEND_NANOS
  uint64_t _batchSendNanos;

  /***
   * versioning
   */
  uint32_t _supportedVersion[5];
  uint32_t _currentVersion;
  int _currentVersionIndex;
  uint32_t _supportedCodecVersion[1];
  uint32_t _currentCodecVersion;
  int _currentCodecVersionIndex;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<logging::Logger> logger_;


};

// Nest Callback Class for write stream
class WriteCallback : public OutputStreamCallback {
 public:
  WriteCallback(DataPacket *packet)
      : _packet(packet) {
  }
  DataPacket *_packet;
  //void process(std::ofstream *stream) {
  int64_t process(std::shared_ptr<io::BaseStream> stream) {
    uint8_t buffer[16384];
    int len = _packet->_size;
    int total = 0;
    while (len > 0) {
	  int size = len < 16384 ? len : 16384; 
      int ret = _packet->transaction_->getStream().readData(buffer, size);
      if (ret != size) {
        logging::LOG_ERROR(_packet->logger_reference_) << "Site2Site Receive Flow Size " << size << " Failed " << ret << ", should have received " << len;
        return -1;
      }
      stream->write(buffer, size);
      len -= size;
      total += size;
    }
    logging::LOG_INFO(_packet->logger_reference_) << "Received " << len << " from stream";
    return len;
  }
};
// Nest Callback Class for read stream
class ReadCallback : public InputStreamCallback {
 public:
  ReadCallback(DataPacket *packet)
      : _packet(packet) {
  }
  DataPacket *_packet;
  int64_t process(std::shared_ptr<io::BaseStream> stream) {
    _packet->_size = 0;
    uint8_t buffer[8192] = { 0 };
    int readSize;
    size_t size = 0;
    do {
      readSize = stream->read(buffer, 8192);

      if (readSize == 0) {
        break;
      }
      if (readSize < 0) {
        return -1;
      }
      int ret = _packet->transaction_->getStream().writeData(buffer, readSize);
      if (ret != readSize) {
        logging::LOG_INFO(_packet->logger_reference_) << "Site2Site Send Flow Size " << readSize << " Failed " << ret;
        return -1;
      }
      size += readSize;
    } while (size < stream->getSize());
    _packet->_size = size;
    return size;
  }
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITECLIENT_H_ */
