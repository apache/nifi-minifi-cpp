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

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Peer.h"
#include "SiteToSite.h"
#include "core/ProcessSession.h"
#include "core/ProcessContext.h"
#include "core/Connectable.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::sitetosite {

/**
 * Represents a piece of data that is to be sent to or that was received from a
 * NiFi instance.
 */
class DataPacket {
 public:
  DataPacket(std::shared_ptr<core::logging::Logger> logger, std::shared_ptr<Transaction> transaction, std::map<std::string, std::string> attributes, const std::string &payload)
      : _attributes{std::move(attributes)},
        transaction_{std::move(transaction)},
        payload_{payload},
        logger_reference_{std::move(logger)} {
  }
  std::map<std::string, std::string> _attributes;
  uint64_t _size{0};
  std::shared_ptr<Transaction> transaction_;
  const std::string & payload_;
  std::shared_ptr<core::logging::Logger> logger_reference_;
};

class SiteToSiteClient : public core::ConnectableImpl {
 public:
  SiteToSiteClient()
      : core::ConnectableImpl("SitetoSiteClient") {
  }

  ~SiteToSiteClient() override = default;

  void setSSLContextService(const std::shared_ptr<minifi::controllers::SSLContextService> &context_service) {
    ssl_context_service_ = context_service;
  }

  /**
   * Creates a transaction using the transaction ID and the direction
   * @param transactionID transaction identifier
   * @param direction direction of transfer
   */
  virtual std::shared_ptr<Transaction> createTransaction(TransferDirection direction) = 0;

  /**
   * Transfers flow files
   * @param direction transfer direction
   * @param context process context
   * @param session process session
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */
  virtual bool transfer(TransferDirection direction, core::ProcessContext& context, core::ProcessSession& session) {
#ifndef WIN32
    if (__builtin_expect(direction == SEND, 1)) {
      return transferFlowFiles(context, session);
    } else {
      return receiveFlowFiles(context, session);
    }
#else
    if (direction == SEND) {
      return transferFlowFiles(context, session);
    } else {
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
  virtual bool transferFlowFiles(core::ProcessContext& context, core::ProcessSession& session);

  /**
   * Receive flow files from server
   * @param context process context
   * @param session process session
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */

  // Confirm the data that was sent or received by comparing CRC32's of the data sent and the data received.
  // Receive flow files for the process session
  bool receiveFlowFiles(core::ProcessContext& context, core::ProcessSession& session);

  // Receive the data packet from the transaction
  // Return false when any error occurs
  bool receive(const utils::Identifier &transactionID, DataPacket *packet, bool &eof);
  /**
   * Transfers raw data and attributes  to server
   * @param context process context
   * @param session process session
   * @param payload data to transmit
   * @param attributes
   * @returns true if the process succeeded, failure OR exception thrown otherwise
   */
  virtual bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload,
                               std::map<std::string, std::string> attributes) = 0;

  void setPortId(utils::Identifier &id) {
    port_id_ = id;
  }

  /**
   * Sets the idle timeout.
   */
  void setIdleTimeout(std::chrono::milliseconds timeout) {
     idle_timeout_ = timeout;
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
  utils::Identifier getPortId() const {
    return port_id_;
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

  const std::shared_ptr<core::logging::Logger> &getLogger() {
    return logger_;
  }

  void yield() override {
  }

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() const override {
    return running_;
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

  virtual bool bootstrap() {
    return true;
  }

  // Return -1 when any error occurs
  virtual int16_t send(const utils::Identifier& transactionID, DataPacket* packet, const std::shared_ptr<core::FlowFile>& flowFile, core::ProcessSession* session);

 protected:
  // Cancel the transaction
  virtual void cancel(const utils::Identifier &transactionID);
  // Complete the transaction
  virtual bool complete(core::ProcessContext& context, const utils::Identifier &transactionID);
  // Error the transaction
  virtual void error(const utils::Identifier &transactionID);

  virtual bool confirm(const utils::Identifier &transactionID);
  // deleteTransaction
  virtual void deleteTransaction(const utils::Identifier &transactionID);

  virtual void tearDown() = 0;

  // read Respond
  virtual int readResponse(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message);
  // write respond
  virtual int writeResponse(const std::shared_ptr<Transaction> &transaction, RespondCode code, const std::string& message);
  // getRespondCodeContext
  virtual RespondCodeContext *getRespondCodeContext(RespondCode code) {
    for (auto & i : SiteToSiteRequest::respondCodeContext) {
      if (i.code == code) {
        return &i;
      }
    }
    return nullptr;
  }

  // Peer State
  PeerState peer_state_{PeerState::IDLE};

  // portId
  utils::Identifier port_id_;

  // idleTimeout
  std::chrono::milliseconds idle_timeout_{15000};

  // Peer Connection
  std::unique_ptr<SiteToSitePeer> peer_;

  std::atomic<bool> running_{false};

  // transaction map
  std::map<utils::Identifier, std::shared_ptr<Transaction>> known_transactions_;

  // BATCH_SEND_NANOS
  std::chrono::nanoseconds _batchSendNanos = std::chrono::seconds(5);

  /***
   * versioning
   */
  uint32_t _supportedVersion[5] = {5, 4, 3, 2, 1};
  int _currentVersionIndex{0};
  uint32_t _currentVersion{_supportedVersion[_currentVersionIndex]};
  uint32_t _supportedCodecVersion[1] = {1};
  int _currentCodecVersionIndex{0};
  uint32_t _currentCodecVersion{_supportedCodecVersion[_currentCodecVersionIndex]};

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<SiteToSiteClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::sitetosite
