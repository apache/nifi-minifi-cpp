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

#pragma once
#include <memory>
#include <string>
#include <utility>

#include "controllers/SSLContextService.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "http/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::curl {
class HTTPClient;
}

namespace org::apache::nifi::minifi::extensions::splunk {

class SplunkHECProcessor : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The ip address or hostname of the Splunk server.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("The HTTP Event Collector HTTP Port Number.")
      .withPropertyType(core::StandardPropertyTypes::PORT_TYPE)
      .withDefaultValue("8088")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Token = core::PropertyDefinitionBuilder<>::createProperty("Token")
      .withDescription("HTTP Event Collector token starting with the string Splunk. For example \'Splunk 1234578-abcd-1234-abcd-1234abcd\'")
      .isRequired(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto SplunkRequestChannel = core::PropertyDefinitionBuilder<>::createProperty("Splunk Request Channel")
      .withDescription("Identifier of the used request channel.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLContext = core::PropertyDefinitionBuilder<0, 0, 1>::createProperty("SSL Context Service")
      .withDescription("The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.")
      .isRequired(false)
      .withExclusiveOfProperties({{{"Hostname", "^http:.*$"}}})
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Hostname,
      Port,
      Token,
      SplunkRequestChannel,
      SSLContext
  });


  explicit SplunkHECProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }
  ~SplunkHECProcessor() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  std::string getNetworkLocation() const;
  static std::shared_ptr<minifi::controllers::SSLContextService> getSSLContextService(core::ProcessContext& context);
  void initializeClient(http::HTTPClient& client, const std::string &url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) const;

  std::string token_;
  std::string hostname_;
  std::string port_;
  std::string request_channel_;
};
}  // namespace org::apache::nifi::minifi::extensions::splunk
