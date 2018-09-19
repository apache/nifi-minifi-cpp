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
#ifndef LIBMINIFI_INCLUDE_SITETOSITE_SITETOSITEFACTORY_H_
#define LIBMINIFI_INCLUDE_SITETOSITE_SITETOSITEFACTORY_H_

#include "RawSocketProtocol.h"
#include "SiteToSite.h"
#include <memory>
#include "Peer.h"
#include "SiteToSiteClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

/**
 * Create a streaming peer from the provided client configuration
 * @param client_configuration client configuration reference
 * @returns SiteToSitePeer
 */
static std::unique_ptr<SiteToSitePeer> createStreamingPeer(const SiteToSiteClientConfiguration &client_configuration) {
  std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str = nullptr;
  if (nullptr != client_configuration.getSecurityContext()) {
    str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
        client_configuration.getStreamFactory()->createSecureSocket(client_configuration.getPeer()->getHost(), client_configuration.getPeer()->getPort(), client_configuration.getSecurityContext()));
  } else {
    str = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
        client_configuration.getStreamFactory()->createSocket(client_configuration.getPeer()->getHost(), client_configuration.getPeer()->getPort()));
  }

  if (nullptr == str)
    return nullptr;
  auto peer = std::unique_ptr<SiteToSitePeer>(new SiteToSitePeer(std::move(str), client_configuration.getPeer()->getHost(), client_configuration.getPeer()->getPort(),
      client_configuration.getInterface()));
  return peer;

}

/**
 * Creates a raw socket client.
 * RawSiteToSiteClient will be instantiated and returned through a unique ptr.
 */
static std::unique_ptr<SiteToSiteClient> createRawSocket(const SiteToSiteClientConfiguration &client_configuration) {
  utils::Identifier uuid;
  client_configuration.getPeer()->getPortId(uuid);
  auto rsptr = createStreamingPeer(client_configuration);
  if (nullptr == rsptr){
    return nullptr;
  }
  auto ptr = std::unique_ptr<SiteToSiteClient>(new RawSiteToSiteClient(std::move(rsptr)));
  ptr->setPortId(uuid);
  ptr->setSSLContextService(client_configuration.getSecurityContext());
  return ptr;
}

/**
 * Returns a client based on the client configuratin's client type.
 * Currently only HTTP and RAW are supported.
 * @param client_configuration client configuration reference
 * @returns site to site client or nullptr.
 */
static std::unique_ptr<SiteToSiteClient> createClient(const SiteToSiteClientConfiguration &client_configuration) {
  utils::Identifier uuid;
  client_configuration.getPeer()->getPortId(uuid);
  switch (client_configuration.getClientType()) {
    case RAW:
      return createRawSocket(client_configuration);
    case HTTP:
      auto http_protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HttpProtocol", "HttpProtocol");
      if (nullptr != http_protocol) {
        auto ptr = std::unique_ptr<SiteToSiteClient>(static_cast<SiteToSiteClient*>(http_protocol));
        ptr->setSSLContextService(client_configuration.getSecurityContext());
        auto peer = std::unique_ptr<SiteToSitePeer>(new SiteToSitePeer(client_configuration.getPeer()->getHost(), client_configuration.getPeer()->getPort(),
            client_configuration.getInterface()));
        peer->setHTTPProxy(client_configuration.getHTTPProxy());

        ptr->setPortId(uuid);
        ptr->setPeer(std::move(peer));
        return ptr;
      }
      return nullptr;
  };
  return nullptr;
}

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_SITETOSITE_SITETOSITEFACTORY_H_ */
