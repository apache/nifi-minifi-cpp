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
#include "client/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::splunk {

void SplunkHECProcessor::initialize() {
  setSupportedProperties(Properties);
}

void SplunkHECProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  if (!context->getProperty(Hostname, hostname_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Hostname");

  if (!context->getProperty(Port, port_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Port");

  if (!context->getProperty(Token, token_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Token");

  if (!context->getProperty(SplunkRequestChannel, request_channel_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get SplunkRequestChannel");
}

std::string SplunkHECProcessor::getNetworkLocation() const {
  return hostname_ + ":" + port_;
}

std::shared_ptr<minifi::controllers::SSLContextService> SplunkHECProcessor::getSSLContextService(core::ProcessContext& context) {
  std::string context_name;
  if (context.getProperty(SSLContext, context_name) && !IsNullOrEmpty(context_name))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(context_name));
  return nullptr;
}

void SplunkHECProcessor::initializeClient(curl::HTTPClient& client, const std::string &url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) const {
  client.initialize("POST", url, std::move(ssl_context_service));
  client.setRequestHeader("Authorization", token_);
  client.setRequestHeader("X-Splunk-Request-Channel", request_channel_);
}

}  // namespace org::apache::nifi::minifi::extensions::splunk
