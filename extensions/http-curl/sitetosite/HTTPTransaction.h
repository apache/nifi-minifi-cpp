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
#ifndef EXTENSIONS_HTTP_CURL_SITETOSITE_HTTPTRANSACTION_H_
#define EXTENSIONS_HTTP_CURL_SITETOSITE_HTTPTRANSACTION_H_

#include "sitetosite/SiteToSite.h"
#include "io/CRCStream.h"
#include "sitetosite/SiteToSiteClient.h"
#include "sitetosite/Peer.h"
#include "HTTPStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

/**
 * Purpose: HTTP Transaction is an implementation that exposes the site to site client.
 * Includes the transaction URL.
 */
class HttpTransaction : public sitetosite::Transaction {
 public:
  explicit HttpTransaction(sitetosite::TransferDirection direction, org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> &stream)
      : Transaction(direction, stream),
        client_ref_(nullptr) {
  }

  ~HttpTransaction(){
    auto stream = dynamic_cast< org::apache::nifi::minifi::io::HttpStream*>(  dynamic_cast<SiteToSitePeer*>(crcStream.getstream())->getStream() );
    stream->forceClose();
  }

  void initialize(sitetosite::SiteToSiteClient *client, const std::string &url) {
    client_ref_ = client;
    transaction_url_ = url;
  }


  const std::string &getTransactionUrl(){
    return transaction_url_;
  }
 protected:
  sitetosite::SiteToSiteClient *client_ref_;
  std::string transaction_url_;
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_SITETOSITE_HTTPTRANSACTION_H_ */
