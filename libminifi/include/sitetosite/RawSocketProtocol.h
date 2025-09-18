/**
 * @file RawSiteToSiteClient.h
 * RawSiteToSiteClient class declaration
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

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/Property.h"
#include "io/CRCStream.h"
#include "Peer.h"
#include "properties/Configure.h"
#include "SiteToSite.h"
#include "SiteToSiteClient.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::sitetosite {


// RawSiteToSiteClient Class
class RawSiteToSiteClient : public sitetosite::SiteToSiteClient {
 public:
  // HandShakeProperty Str
  static const char *HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY];

  RawSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer) { // NOLINT
    peer_ = std::move(peer);
    _batchSize = 0;
    _batchCount = 0;
    _batchDuration = std::chrono::seconds(0);
    _batchSendNanos = std::chrono::seconds(5);
    _timeout = std::chrono::seconds(30);
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

  ~RawSiteToSiteClient() override {
    tearDown();
  }

 public:
  // setBatchSize
  void setBatchSize(uint64_t size) {
    _batchSize = size;
  }
  // setBatchCount
  void setBatchCount(uint64_t count) {
    _batchCount = count;
  }
  // setBatchDuration
  void setBatchDuration(std::chrono::milliseconds duration) {
    _batchDuration = duration;
  }
  // setTimeout
  void setTimeout(std::chrono::milliseconds time) {
    _timeout = time;
    if (peer_)
      peer_->setTimeout(time);
  }

  /**
   * Provides a reference to the time out
   * @returns timeout
   */
  std::chrono::milliseconds getTimeout() const {
    return _timeout;
  }

  // getResourceName
  std::string getResourceName() {
    return "SocketFlowFileProtocol";
  }
  // getCodecResourceName
  std::string getCodecResourceName() {
    return "StandardFlowFileCodec";
  }

  // get peerList
  bool getPeerList(std::vector<PeerStatus> &peer) override;
  // negotiateCodec
  virtual bool negotiateCodec();
  // initiateResourceNegotiation
  virtual bool initiateResourceNegotiation();
  // initiateCodecResourceNegotiation
  virtual bool initiateCodecResourceNegotiation();
  // tearDown
  void tearDown() override;
  // write Request Type
  virtual int writeRequestType(RequestType type);
  // read Request Type
  virtual int readRequestType(RequestType &type);

  // read Respond
  virtual int readRespond(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message);
  // write respond
  virtual int writeRespond(const std::shared_ptr<Transaction> &transaction, RespondCode code, const std::string& message);
  // getRespondCodeContext
  RespondCodeContext *getRespondCodeContext(RespondCode code) override {
    return SiteToSiteClient::getRespondCodeContext(code);
  }

  // Creation of a new transaction, return the transaction ID if success,
  // Return NULL when any error occurs
  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;

  //! Transfer string for the process session
  bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload,
      std::map<std::string, std::string> attributes) override;

  // bootstrap the protocol to the ready for transaction state by going through the state machine
  bool bootstrap() override;

 protected:
  // establish
  bool establish() override;
  // handShake
  virtual bool handShake();

 private:
  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RawSiteToSiteClient>::getLogger();
  // Batch Count
  std::atomic<uint64_t> _batchCount;
  // Batch Size
  std::atomic<uint64_t> _batchSize;
  // Batch Duration in msec
  std::atomic<std::chrono::milliseconds> _batchDuration;
  // Timeout in msec
  std::atomic<std::chrono::milliseconds> _timeout;

  // commsIdentifier
  utils::Identifier _commsIdentifier;

  RawSiteToSiteClient(const RawSiteToSiteClient &parent);
  RawSiteToSiteClient &operator=(const RawSiteToSiteClient &parent);
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::sitetosite
