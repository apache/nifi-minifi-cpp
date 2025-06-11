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

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/logging/LoggerFactory.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "FlowFileRecord.h"
#include "io/CRCStream.h"
#include "Peer.h"
#include "SiteToSite.h"
#include "SiteToSiteClient.h"
#include "utils/Id.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

namespace test {
class SiteToSiteClientTestAccessor;
}  // namespace test

namespace sitetosite {

class RawSiteToSiteClient final : public SiteToSiteClient {
 public:
  explicit RawSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer) : SiteToSiteClient(std::move(peer)) {
    timeout_ = 30s;
  }

  RawSiteToSiteClient(const RawSiteToSiteClient &parent) = delete;
  RawSiteToSiteClient &operator=(const RawSiteToSiteClient &parent) = delete;
  RawSiteToSiteClient(RawSiteToSiteClient &&parent) = delete;
  RawSiteToSiteClient &operator=(RawSiteToSiteClient &&parent) = delete;

  ~RawSiteToSiteClient() override {
    tearDown();
  }

 public:
  void setTimeout(std::chrono::milliseconds time) override {
    timeout_ = time;
    if (peer_) {
      peer_->setTimeout(time);
    }
  }

  std::optional<std::vector<PeerStatus>> getPeerList() override;
  bool transmitPayload(core::ProcessContext& context, const std::string &payload, const std::map<std::string, std::string>& attributes) override;

 protected:
  friend class test::SiteToSiteClientTestAccessor;
  bool bootstrap() override;
  bool establish() override;
  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;
  void tearDown() override;

 private:
  static constexpr std::string_view PROTOCOL_RESOURCE_NAME = "SocketFlowFileProtocol";
  static constexpr std::string_view CODEC_RESOURCE_NAME = "StandardFlowFileCodec";

  bool handShake();
  bool negotiateCodec();
  bool initiateResourceNegotiation();
  bool initiateCodecResourceNegotiation();
  int64_t writeRequestType(RequestType type);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RawSiteToSiteClient>::getLogger();
  utils::Identifier comms_identifier_;
};

}  // namespace sitetosite
}  // namespace org::apache::nifi::minifi
