// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "SplunkHECProcessor.h"

#include <utility>

#include "minifi-cpp/core/ProcessContext.h"
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

void SplunkHECProcessor::initializeClient(http::HTTPClient& client, const std::string &url, std::shared_ptr<minifi::controllers::SSLContextServiceInterface> ssl_context_service) const {
  client.initialize(http::HttpRequestMethod::Post, url, std::move(ssl_context_service));
  client.setRequestHeader("Authorization", token_);
  client.setRequestHeader("X-Splunk-Request-Channel", request_channel_);
}

}  // namespace org::apache::nifi::minifi::extensions::splunk
