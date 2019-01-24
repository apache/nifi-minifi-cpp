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
#ifndef __SITE2SITE_CLIENT_PROTOCOL_H__
#define __SITE2SITE_CLIENT_PROTOCOL_H__

#include <stdio.h>
#include <sys/types.h>
#include <string>
#include <errno.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <uuid/uuid.h>

#include "SiteToSite.h"
#include "SiteToSiteClient.h"
#include "core/Property.h"
#include "properties/Configure.h"
#include "FlowFileRecord.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "io/CRCStream.h"
#include "Peer.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {


/**
 * Site2Site Peer
 */
typedef struct Site2SitePeerStatus {
  std::string host_;
  int port_;bool isSecure_;
} Site2SitePeerStatus;

// RawSiteToSiteClient Class
class RawSiteToSiteClient : public sitetosite::SiteToSiteClient {
 public:

  // HandShakeProperty Str
  static const char *HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY];

  // Constructor
  /*!
   * Create a new control protocol
   */
  RawSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer)
      : logger_(logging::LoggerFactory<RawSiteToSiteClient>::getLogger()) {
    peer_ = std::move(peer);
    _batchSize = 0;
    _batchCount = 0;
    _batchDuration = 0;
    _batchSendNanos = 5000000000;  // 5 seconds
    _timeOut = 30000;  // 30 seconds
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
  // Destructor
  virtual ~RawSiteToSiteClient() {
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
  void setBatchDuration(uint64_t duration) {
    _batchDuration = duration;
  }
  // setTimeOut
  void setTimeOut(uint64_t time) {
    _timeOut = time;
    if (peer_)
      peer_->setTimeOut(time);

  }

  void setPeer(std::unique_ptr<SiteToSitePeer> peer) {
    peer_ = std::move(peer);
  }
  /**
   * Provides a reference to the time out
   * @returns timeout
   */
  const uint64_t getTimeOut() const {
    return _timeOut;
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
  virtual bool getPeerList(std::vector<PeerStatus> &peer);
  // negotiateCodec
  virtual bool negotiateCodec();
  // initiateResourceNegotiation
  virtual bool initiateResourceNegotiation();
  // initiateCodecResourceNegotiation
  virtual bool initiateCodecResourceNegotiation();
  // tearDown
  virtual void tearDown();
  // write Request Type
  virtual int writeRequestType(RequestType type);
  // read Request Type
  virtual int readRequestType(RequestType &type);
  // read Respond
  virtual int readRespond(const std::shared_ptr<Transaction> &transaction, RespondCode &code, std::string &message);
  // write respond
  virtual int writeRespond(const std::shared_ptr<Transaction> &transaction, RespondCode code, std::string message);
  // getRespondCodeContext
  virtual RespondCodeContext *getRespondCodeContext(RespondCode code) {
    for (unsigned int i = 0; i < sizeof(SiteToSiteRequest::respondCodeContext) / sizeof(RespondCodeContext); i++) {
      if (SiteToSiteRequest::respondCodeContext[i].code == code) {
        return &SiteToSiteRequest::respondCodeContext[i];
      }
    }
    return NULL;
  }

  // Creation of a new transaction, return the transaction ID if success,
  // Return NULL when any error occurs
  virtual std::shared_ptr<Transaction> createTransaction(std::string &transactionID, TransferDirection direction);

  //! Transfer string for the process session
  virtual bool transmitPayload(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session, const std::string &payload,
                               std::map<std::string, std::string> attributes);

  // bootstrap the protocol to the ready for transaction state by going through the state machine
  virtual bool bootstrap();
 protected:

  // establish
  virtual bool establish();
  // handShake
  virtual bool handShake();

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Batch Count
  std::atomic<uint64_t> _batchCount;
  // Batch Size
  std::atomic<uint64_t> _batchSize;
  // Batch Duration in msec
  std::atomic<uint64_t> _batchDuration;
  // Timeout in msec
  std::atomic<uint64_t> _timeOut;

  // commsIdentifier
  std::string _commsIdentifier;

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  RawSiteToSiteClient(const RawSiteToSiteClient &parent);
  RawSiteToSiteClient &operator=(const RawSiteToSiteClient &parent);
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
