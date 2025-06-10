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

#include <utility>
#include <string>

#include "sitetosite/SiteToSite.h"
#include "io/CRCStream.h"
#include "sitetosite/SiteToSiteClient.h"
#include "sitetosite/Peer.h"
#include "http/HTTPStream.h"

namespace org::apache::nifi::minifi::sitetosite {

class HttpTransaction final : public sitetosite::Transaction {
 public:
  explicit HttpTransaction(sitetosite::TransferDirection direction, org::apache::nifi::minifi::io::CRCStream<sitetosite::SiteToSitePeer> &&stream)
      : Transaction(direction, std::move(stream)) {}

  HttpTransaction(const HttpTransaction&) = delete;
  HttpTransaction& operator=(const HttpTransaction&) = delete;
  HttpTransaction(HttpTransaction&&) = delete;
  HttpTransaction& operator=(HttpTransaction&&) = delete;

  ~HttpTransaction() override {
    if (auto stream = dynamic_cast<http::HttpStream*>(dynamic_cast<sitetosite::SiteToSitePeer*>(crc_stream_.getstream())->getStream())) {
      stream->forceClose();
    }
  }

  void initialize(sitetosite::SiteToSiteClient* client, const std::string &url) {
    client_ = client;
    transaction_url_ = url;
  }

  [[nodiscard]] const std::string &getTransactionUrl() const {
    return transaction_url_;
  }

 protected:
  sitetosite::SiteToSiteClient* client_ = nullptr;
  std::string transaction_url_;
};

}  // namespace org::apache::nifi::minifi::sitetosite
