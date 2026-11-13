// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
