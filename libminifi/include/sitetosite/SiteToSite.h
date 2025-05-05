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

#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/controllers/SSLContextService.h"
#include "Peer.h"
#include "core/Property.h"
#include "properties/Configure.h"
#include "io/CRCStream.h"
#include "utils/Id.h"
#include "http/BaseHTTPClient.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::sitetosite {

enum class ResourceNegotiationStatusCode : uint8_t {
  RESOURCE_OK = 20,
  DIFFERENT_RESOURCE_VERSION = 21,
  NEGOTIATED_ABORT = 255
};

static constexpr uint32_t MAX_NUM_ATTRIBUTES = 25000;

// Response Code Sequence Pattern
static constexpr uint8_t CODE_SEQUENCE_VALUE_1 = static_cast<uint8_t>('R');
static constexpr uint8_t CODE_SEQUENCE_VALUE_2 = static_cast<uint8_t>('C');

/**
 * Enumeration of Properties that can be used for the Site-to-Site Socket
 * Protocol.
 */
enum class HandshakeProperty {
  /**
   * Boolean value indicating whether or not the contents of a FlowFile should
   * be GZipped when transferred.
   */
  GZIP,
  /**
   * The unique identifier of the port to communicate with
   */
  PORT_IDENTIFIER,
  /**
   * Indicates the number of milliseconds after the request was made that the
   * client will wait for a response. If no response has been received by the
   * time this value expires, the server can move on without attempting to
   * service the request because the client will have already disconnected.
   */
  REQUEST_EXPIRATION_MILLIS,
  /**
   * The preferred number of FlowFiles that the server should send to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol.
   */
  BATCH_COUNT,
  /**
   * The preferred number of bytes that the server should send to the client
   * when pulling data. This property was introduced in version 5 of the
   * protocol.
   */
  BATCH_SIZE,
  /**
   * The preferred amount of time that the server should send data to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol. Value is in milliseconds.
   */
  BATCH_DURATION,
  MAX_HANDSHAKE_PROPERTY
};

enum class ClientType {
  RAW,
  HTTP
};

enum class TransferDirection {
  SEND,
  RECEIVE
};

enum class PeerState {
  IDLE = 0,
  ESTABLISHED,
  HANDSHAKED,
  READY
};

enum class TransactionState {
  TRANSACTION_STARTED,
  DATA_EXCHANGED,
  TRANSACTION_CONFIRMED,
  TRANSACTION_COMPLETED,
  TRANSACTION_CANCELED,
  TRANSACTION_CLOSED,
  TRANSACTION_ERROR
};

enum class RequestType {
  NEGOTIATE_FLOWFILE_CODEC = 0,
  REQUEST_PEER_LIST,
  SEND_FLOWFILES,
  RECEIVE_FLOWFILES,
  SHUTDOWN
};

enum class ResponseCode : uint8_t {
  RESERVED = 0,
  // ResponseCode, so that we can indicate a 0 followed by some other bytes

  // handshaking properties
  PROPERTIES_OK = 1,
  UNKNOWN_PROPERTY_NAME = 230,
  ILLEGAL_PROPERTY_VALUE = 231,
  MISSING_PROPERTY = 232,
  // transaction indicators
  CONTINUE_TRANSACTION = 10,
  FINISH_TRANSACTION = 11,
  CONFIRM_TRANSACTION = 12,  // "Explanation" of this code is the checksum
  TRANSACTION_FINISHED = 13,
  TRANSACTION_FINISHED_BUT_DESTINATION_FULL = 14,
  CANCEL_TRANSACTION = 15,
  BAD_CHECKSUM = 19,
  // data availability indicators
  MORE_DATA = 20,
  NO_MORE_DATA = 21,
  // port state indicators
  UNKNOWN_PORT = 200,
  PORT_NOT_IN_VALID_STATE = 201,
  PORTS_DESTINATION_FULL = 202,
  // authorization
  UNAUTHORIZED = 240,
  // error indicators
  ABORT = 250,
  UNRECOGNIZED_RESPONSE_CODE = 254,
  END_OF_STREAM = 255
};

struct ResponseCodeContext {
  ResponseCode code = ResponseCode::UNRECOGNIZED_RESPONSE_CODE;
  const std::string_view description;
  bool has_description = false;
};

static constexpr std::array<ResponseCodeContext, 21> respond_code_contexts = {{
  { ResponseCode::RESERVED, "Reserved for Future Use", false },
  { ResponseCode::PROPERTIES_OK, "Properties OK", false },
  { ResponseCode::UNKNOWN_PROPERTY_NAME, "Unknown Property Name", true },
  { ResponseCode::ILLEGAL_PROPERTY_VALUE, "Illegal Property Value", true },
  { ResponseCode::MISSING_PROPERTY, "Missing Property", true },
  { ResponseCode::CONTINUE_TRANSACTION, "Continue Transaction", false },
  { ResponseCode::FINISH_TRANSACTION, "Finish Transaction", false },
  { ResponseCode::CONFIRM_TRANSACTION, "Confirm Transaction", true },
  { ResponseCode::TRANSACTION_FINISHED, "Transaction Finished", false },
  { ResponseCode::TRANSACTION_FINISHED_BUT_DESTINATION_FULL, "Transaction Finished But Destination is Full", false },
  { ResponseCode::CANCEL_TRANSACTION, "Cancel Transaction", true },
  { ResponseCode::BAD_CHECKSUM, "Bad Checksum", false },
  { ResponseCode::MORE_DATA, "More Data Exists", false },
  { ResponseCode::NO_MORE_DATA, "No More Data Exists", false },
  { ResponseCode::UNKNOWN_PORT, "Unknown Port", false },
  { ResponseCode::PORT_NOT_IN_VALID_STATE, "Port Not in a Valid State", true },
  { ResponseCode::PORTS_DESTINATION_FULL, "Port's Destination is Full", false },
  { ResponseCode::UNAUTHORIZED, "User Not Authorized", true },
  { ResponseCode::ABORT, "Abort", true },
  { ResponseCode::UNRECOGNIZED_RESPONSE_CODE, "Unrecognized Response Code", false },
  { ResponseCode::END_OF_STREAM, "End of Stream", false }
}};

class Transaction {
 public:
  explicit Transaction(TransferDirection direction, org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> &&stream)
      : closed_(false),
        crc_stream_(std::move(stream)),
        uuid_(utils::IdGenerator::getIdGenerator()->generate()) {
    state_ = TransactionState::TRANSACTION_STARTED;
    direction_ = direction;
    data_available_ = false;
    current_transfers_ = 0;
    total_transfers_ = 0;
    bytes_ = 0;
  }

  virtual ~Transaction() = default;

  utils::SmallString<36> getUUIDStr() const {
    return uuid_.to_string();
  }

  utils::Identifier getUUID() const {
    return uuid_;
  }

  void setTransactionId(const utils::Identifier& id) {
    uuid_ = id;
  }

  void setState(TransactionState state) {
    state_ = state;
  }

  TransactionState getState() const {
    return state_;
  }

  bool isDataAvailable() const {
    return data_available_;
  }

  void setDataAvailable(bool value) {
    data_available_ = value;
  }

  TransferDirection getDirection() const {
    return direction_;
  }

  uint64_t getCRC() const {
    return crc_stream_.getCRC();
  }

  void updateCRC(uint8_t *buffer, uint32_t length) {
    crc_stream_.updateCRC(buffer, length);
  }

  org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer>& getStream() {
    return crc_stream_;
  }

  uint64_t getCurrentTransfers() const {
    return current_transfers_;
  }

  uint64_t getTotalTransfers() const {
    return total_transfers_;
  }

  uint64_t getBytes() const {
    return bytes_;
  }

  void addBytes(uint64_t bytes) {
    bytes_ += bytes;
  }

  void incrementCurrentTransfers() {
    current_transfers_++;
  }

  void decrementCurrentTransfers() {
    if (current_transfers_ > 0) {
      current_transfers_--;
    }
  }

  void incrementTotalTransfers() {
    total_transfers_++;
  }

  bool isClosed() const {
    return closed_;
  }

  void close() {
    closed_ = true;
  }

  Transaction(const Transaction &parent) = delete;
  Transaction &operator=(const Transaction &parent) = delete;

 protected:
  uint64_t current_transfers_;
  uint64_t total_transfers_;
  uint64_t bytes_;
  TransactionState state_;
  bool closed_;
  bool data_available_;
  org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crc_stream_;

 private:
  TransferDirection direction_;
  utils::Identifier uuid_;
};

class SiteToSiteClientConfiguration {
 public:
  SiteToSiteClientConfiguration(const utils::Identifier &port_id, std::string host, uint16_t port, const std::string &ifc, ClientType type)
      : port_id_(port_id),
        host_(std::move(host)),
        port_(port),
        local_network_interface_(ifc),
        ssl_service_(nullptr) {
    client_type_ = type;
  }

  SiteToSiteClientConfiguration(const SiteToSiteClientConfiguration &other) = delete;
  SiteToSiteClientConfiguration &operator=(const SiteToSiteClientConfiguration &other) = delete;
  SiteToSiteClientConfiguration(SiteToSiteClientConfiguration &&other) = delete;
  SiteToSiteClientConfiguration &operator=(SiteToSiteClientConfiguration &&other) = delete;
  ~SiteToSiteClientConfiguration() = default;

  ClientType getClientType() const {
    return client_type_;
  }

  const utils::Identifier& getPortId() const {
    return port_id_;
  }

  const std::string& getHost() const {
    return host_;
  }

  uint16_t getPort() const {
    return port_;
  }

  void setSecurityContext(const std::shared_ptr<controllers::SSLContextService> &ssl_service) {
    ssl_service_ = ssl_service;
  }

  const std::shared_ptr<controllers::SSLContextService> &getSecurityContext() const {
    return ssl_service_;
  }

  void setIdleTimeout(std::chrono::milliseconds timeout) {
    idle_timeout_ = timeout;
  }

  std::chrono::milliseconds getIdleTimeout() const {
    return idle_timeout_;
  }

  void setInterface(const std::string &ifc) {
    local_network_interface_ = ifc;
  }

  std::string getInterface() const {
    return local_network_interface_;
  }

  void setHTTPProxy(const http::HTTPProxy &proxy) {
    proxy_ = proxy;
  }

  http::HTTPProxy getHTTPProxy() const {
    return proxy_;
  }

  void setUseCompression(bool use_compression) {
    use_compression_ = use_compression;
  }

  bool getUseCompression() const {
    return use_compression_;
  }

  void setBatchCount(std::optional<uint64_t> count) {
    batch_count_ = count;
  }

  std::optional<uint64_t> getBatchCount() const {
    return batch_count_;
  }

  void setBatchSize(std::optional<uint64_t> size) {
    batch_size_ = size;
  }

  std::optional<uint64_t> getBatchSize() const {
    return batch_size_;
  }

  void setBatchDuration(std::optional<std::chrono::milliseconds> duration) {
    batch_duration_ = duration;
  }

  std::optional<std::chrono::milliseconds> getBatchDuration() const {
    return batch_duration_;
  }

  void setTimeout(std::optional<std::chrono::milliseconds> timeout) {
    timeout_ = timeout;
  }

  std::optional<std::chrono::milliseconds> getTimeout() const {
    return timeout_;
  }

 private:
  utils::Identifier port_id_;
  std::string host_;
  uint16_t port_;
  ClientType client_type_;
  std::string local_network_interface_;
  std::chrono::milliseconds idle_timeout_{15000};
  std::shared_ptr<controllers::SSLContextService> ssl_service_;
  http::HTTPProxy proxy_;
  bool use_compression_{false};
  std::optional<uint64_t> batch_count_;
  std::optional<uint64_t> batch_size_;
  std::optional<std::chrono::milliseconds> batch_duration_;
  std::optional<std::chrono::milliseconds> timeout_;
};

}  // namespace org::apache::nifi::minifi::sitetosite
