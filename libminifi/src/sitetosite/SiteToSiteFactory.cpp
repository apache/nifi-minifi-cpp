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
#include "sitetosite/SiteToSiteFactory.h"

#include <utility>
#include <memory>

#include "sitetosite/RawSiteToSiteClient.h"
#include "sitetosite/HttpSiteToSiteClient.h"
#include "utils/net/AsioSocketUtils.h"
#include "core/ClassLoader.h"

namespace org::apache::nifi::minifi::sitetosite {

namespace {
std::unique_ptr<SiteToSitePeer> createStreamingPeer(const SiteToSiteClientConfiguration &client_configuration) {
  utils::net::SocketData socket_data{client_configuration.getHost(), client_configuration.getPort(), client_configuration.getSecurityContext()};
  auto connection = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  return std::make_unique<SiteToSitePeer>(std::move(connection), client_configuration.getHost(), client_configuration.getPort(), client_configuration.getInterface());
}

void setCommonConfigurationOptions(SiteToSiteClient& client, const SiteToSiteClientConfiguration &client_configuration) {
  client.setSSLContextService(client_configuration.getSecurityContext());
  client.setUseCompression(client_configuration.getUseCompression());
  if (client_configuration.getBatchCount()) {
    client.setBatchCount(client_configuration.getBatchCount().value());
  }
  if (client_configuration.getBatchSize()) {
    client.setBatchSize(client_configuration.getBatchSize().value());
  }
  if (client_configuration.getBatchDuration()) {
    client.setBatchDuration(client_configuration.getBatchDuration().value());
  }
  if (client_configuration.getTimeout()) {
    client.setTimeout(client_configuration.getTimeout().value());
  }
}

std::unique_ptr<SiteToSiteClient> createRawSocketSiteToSiteClient(const SiteToSiteClientConfiguration &client_configuration) {
  auto raw_site_to_site_client = std::make_unique<RawSiteToSiteClient>(createStreamingPeer(client_configuration));
  raw_site_to_site_client->setPortId(client_configuration.getPortId());
  setCommonConfigurationOptions(*raw_site_to_site_client, client_configuration);
  return raw_site_to_site_client;
}

std::unique_ptr<SiteToSiteClient> createHttpSiteToSiteClient(const SiteToSiteClientConfiguration &client_configuration) {
  auto peer = std::make_unique<SiteToSitePeer>(client_configuration.getHost(), client_configuration.getPort(), client_configuration.getInterface());
  peer->setHTTPProxy(client_configuration.getHTTPProxy());

  auto http_site_to_site_client = std::make_unique<HttpSiteToSiteClient>(std::move(peer));
  http_site_to_site_client->setPortId(client_configuration.getPortId());
  http_site_to_site_client->setIdleTimeout(client_configuration.getIdleTimeout());
  setCommonConfigurationOptions(*http_site_to_site_client, client_configuration);
  return http_site_to_site_client;
}
}  // namespace

std::unique_ptr<SiteToSiteClient> createClient(const SiteToSiteClientConfiguration &client_configuration) {
  switch (client_configuration.getClientType()) {
    case ClientType::RAW:
      return createRawSocketSiteToSiteClient(client_configuration);
    case ClientType::HTTP:
      return createHttpSiteToSiteClient(client_configuration);
  }
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::sitetosite
