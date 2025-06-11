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
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <optional>

#include "Peer.h"
#include "SiteToSite.h"
#include "core/ProcessSession.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi {

namespace test {
class SiteToSiteClientTestAccessor;
}  // namespace test

namespace sitetosite {

struct DataPacket {
 public:
  DataPacket(std::shared_ptr<Transaction> transaction, const std::string &payload)
      : transaction{std::move(transaction)},
        payload{payload} {
  }
  DataPacket(std::shared_ptr<Transaction> transaction, std::map<std::string, std::string> attributes, const std::string &payload)
      : attributes{std::move(attributes)},
        transaction{std::move(transaction)},
        payload{payload} {
  }
  std::map<std::string, std::string> attributes;
  uint64_t size{0};
  std::shared_ptr<Transaction> transaction;
  const std::string& payload;
};

struct SiteToSiteResponse {
  ResponseCode code = ResponseCode::UNRECOGNIZED_RESPONSE_CODE;
  std::string message;
};

class SiteToSiteClient {
 public:
  explicit SiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer)
      : peer_(std::move(peer)) {
    gsl_Assert(peer_);
  }

  SiteToSiteClient(const SiteToSiteClient&) = delete;
  SiteToSiteClient(SiteToSiteClient&&) = delete;
  SiteToSiteClient& operator=(const SiteToSiteClient&) = delete;
  SiteToSiteClient& operator=(SiteToSiteClient&&) = delete;

  virtual ~SiteToSiteClient() = default;

  virtual std::optional<std::vector<PeerStatus>> getPeerList() = 0;
  virtual bool transmitPayload(core::ProcessContext& context, const std::string &payload, const std::map<std::string, std::string>& attributes) = 0;

  bool transfer(TransferDirection direction, core::ProcessContext& context, core::ProcessSession& session) {
    if (direction == TransferDirection::SEND) {
      return transferFlowFiles(context, session);
    } else {
      return receiveFlowFiles(context, session);
    }
  }

  void setPortId(const utils::Identifier& id) {
    port_id_ = id;
  }

  void setIdleTimeout(std::chrono::milliseconds timeout) {
     idle_timeout_ = timeout;
  }

  [[nodiscard]] utils::Identifier getPortId() const {
    return port_id_;
  }

  [[nodiscard]] const std::shared_ptr<core::logging::Logger> &getLogger() {
    return logger_;
  }

  void setSSLContextService(const std::shared_ptr<minifi::controllers::SSLContextService> &context_service) {
    ssl_context_service_ = context_service;
  }

  void setUseCompression(bool use_compression) {
    use_compression_ = use_compression;
  }

  void setBatchSize(uint64_t size) {
    batch_size_ = size;
  }

  void setBatchCount(uint64_t count) {
    batch_count_ = count;
  }

  void setBatchDuration(std::chrono::milliseconds duration) {
    batch_duration_ = duration;
  }

  virtual void setTimeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
  }

 protected:
  friend class test::SiteToSiteClientTestAccessor;

  virtual bool bootstrap() = 0;
  virtual bool establish() = 0;
  virtual std::shared_ptr<Transaction> createTransaction(TransferDirection direction) = 0;
  virtual void tearDown() = 0;

  virtual void deleteTransaction(const utils::Identifier &transaction_id);
  virtual std::optional<SiteToSiteResponse> readResponse(const std::shared_ptr<Transaction> &transaction);
  virtual bool writeResponse(const std::shared_ptr<Transaction> &transaction, const SiteToSiteResponse& response);

  bool initializeSend(const std::shared_ptr<Transaction>& transaction);
  bool writeAttributesInSendTransaction(const std::shared_ptr<Transaction>& transaction, const std::map<std::string, std::string>& attributes);
  void finalizeSendTransaction(const std::shared_ptr<Transaction>& transaction, uint64_t sent_bytes);
  bool sendPacket(const DataPacket& packet);
  bool sendFlowFile(const std::shared_ptr<Transaction>& transaction, core::FlowFile& flow_file, core::ProcessSession& session);

  void cancel(const utils::Identifier &transaction_id);
  bool complete(core::ProcessContext& context, const utils::Identifier &transaction_id);
  void error(const utils::Identifier &transaction_id);
  bool confirm(const utils::Identifier &transaction_id);

  void handleTransactionError(const std::shared_ptr<Transaction>& transaction, core::ProcessContext& context, const std::exception& exception);

  PeerState peer_state_{PeerState::IDLE};
  utils::Identifier port_id_;
  std::chrono::milliseconds idle_timeout_{15000};
  std::unique_ptr<SiteToSitePeer> peer_;
  std::map<utils::Identifier, std::shared_ptr<Transaction>> known_transactions_;
  std::chrono::nanoseconds batch_send_nanos_{5s};

  const std::vector<uint32_t> supported_version_ = {5, 4, 3, 2, 1};
  uint32_t current_version_index_{0};
  uint32_t current_version_{supported_version_[current_version_index_]};
  const std::vector<uint32_t> supported_codec_version_ = {1};
  uint32_t current_codec_version_index_{0};
  uint32_t current_codec_version_{supported_codec_version_[current_codec_version_index_]};

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

  std::atomic_bool use_compression_{false};
  std::atomic<uint64_t> batch_count_{0};
  std::atomic<uint64_t> batch_size_{0};
  std::atomic<std::chrono::milliseconds> batch_duration_{0s};
  std::atomic<std::chrono::milliseconds> timeout_{0s};

 private:
  struct ReceiveFlowFileHeaderResult {
    std::map<std::string, std::string> attributes;
    size_t flow_file_data_size = 0;
    bool eof{false};
  };

  static const ResponseCodeContext* getResponseCodeContext(ResponseCode code);
  bool transferFlowFiles(core::ProcessContext& context, core::ProcessSession& session);
  bool receiveFlowFiles(core::ProcessContext& context, core::ProcessSession& session);

  bool confirmReceive(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id);
  bool confirmSend(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id);
  bool completeReceive(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id);
  bool completeSend(const std::shared_ptr<Transaction>& transaction, const utils::Identifier& transaction_id, core::ProcessContext& context);

  bool readFlowFileHeaderData(const std::shared_ptr<Transaction>& transaction, SiteToSiteClient::ReceiveFlowFileHeaderResult& result);
  std::optional<ReceiveFlowFileHeaderResult> receiveFlowFileHeader(const std::shared_ptr<Transaction>& transaction);
  std::pair<uint64_t, uint64_t> readFlowFiles(const std::shared_ptr<Transaction>& transaction, core::ProcessSession& session);

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<SiteToSiteClient>::getLogger()};
};

}  // namespace sitetosite
}  // namespace org::apache::nifi::minifi
