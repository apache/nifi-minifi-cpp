/**
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

#include "SplunkHECProcessor.h"

#include <utility>

#include "core/ProcessContext.h"
#include "http/HTTPClient.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::splunk {

void SplunkHECProcessor::initialize() {
  setSupportedProperties(Properties);
}

void SplunkHECProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  hostname_ = utils::parseProperty(context, Hostname);
  port_ = utils::parseProperty(context, Port);
  token_ = utils::parseProperty(context, Token);
  request_channel_ = utils::parseProperty(context, SplunkRequestChannel);
}

std::string SplunkHECProcessor::getNetworkLocation() const {
  return hostname_ + ":" + port_;
}

std::shared_ptr<minifi::controllers::SSLContextServiceInterface> SplunkHECProcessor::getSSLContextService(core::ProcessContext& context) const {
  if (const auto context_name = context.getProperty(SSLContext); context_name && !IsNullOrEmpty(*context_name))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextServiceInterface>(context.getControllerService(*context_name, getUUID()));
  return nullptr;
}

void SplunkHECProcessor::initializeClient(http::HTTPClient& client, const std::string &url, std::shared_ptr<minifi::controllers::SSLContextServiceInterface> ssl_context_service) const {
  client.initialize(http::HttpRequestMethod::POST, url, std::move(ssl_context_service));
  client.setRequestHeader("Authorization", token_);
  client.setRequestHeader("X-Splunk-Request-Channel", request_channel_);
}

}  // namespace org::apache::nifi::minifi::extensions::splunk
