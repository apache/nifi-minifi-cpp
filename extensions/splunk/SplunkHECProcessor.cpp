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
#include "client/HTTPClient.h"
#include "utils/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::splunk {

const core::Property SplunkHECProcessor::Hostname(core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The ip address or hostname of the Splunk server.")
    ->isRequired(true)->build());

const core::Property SplunkHECProcessor::Port(core::PropertyBuilder::createProperty("Port")
    ->withDescription("The HTTP Event Collector HTTP Port Number.")
    ->withDefaultValue("8088")->isRequired(true)->build());

const core::Property SplunkHECProcessor::Token(core::PropertyBuilder::createProperty("Token")
    ->withDescription("HTTP Event Collector token starting with the string Splunk. For example \'Splunk 1234578-abcd-1234-abcd-1234abcd\'")
    ->isRequired(true)->build());

const core::Property SplunkHECProcessor::SplunkRequestChannel(core::PropertyBuilder::createProperty("Splunk Request Channel")
    ->withDescription("Identifier of the used request channel.")->isRequired(true)->build());

const core::Property SplunkHECProcessor::SSLContext(core::PropertyBuilder::createProperty("SSL Context Service")
    ->withDescription("The SSL Context Service used to provide client certificate "
                      "information for TLS/SSL (https) connections.")
    ->isRequired(false)->withExclusiveProperty("Remote URL", "^http:.*$")
    ->asType<minifi::controllers::SSLContextService>()->build());

void SplunkHECProcessor::initialize() {
  setSupportedProperties({Hostname, Port, Token, SplunkRequestChannel});
}

void SplunkHECProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  if (!context->getProperty(Hostname.getName(), hostname_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Hostname");

  if (!context->getProperty(Port.getName(), port_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Port");

  if (!context->getProperty(Token.getName(), token_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get Token");

  if (!context->getProperty(SplunkRequestChannel.getName(), request_channel_))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to get SplunkRequestChannel");
}

std::string SplunkHECProcessor::getNetworkLocation() const {
  return hostname_ + ":" + port_;
}

std::shared_ptr<minifi::controllers::SSLContextService> SplunkHECProcessor::getSSLContextService(core::ProcessContext& context) const {
  std::string context_name;
  if (context.getProperty(SSLContext.getName(), context_name) && !IsNullOrEmpty(context_name))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(context_name));
  return nullptr;
}

void SplunkHECProcessor::setHeaders(utils::HTTPClient& client) const {
  client.initialize("POST");
  client.appendHeader("Authorization", token_);
  client.appendHeader("X-Splunk-Request-Channel", request_channel_);
}

}  // namespace org::apache::nifi::minifi::extensions::splunk
