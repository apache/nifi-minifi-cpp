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
#include <optional>
#include <string>
#include <utility>

#include "controllers/SSLContextService.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "rdkafka_utils.h"
#include "utils/Enum.h"
#include "utils/net/Ssl.h"

namespace org::apache::nifi::minifi::processors {

namespace kafka {
SMART_ENUM(SecurityProtocolOption,
  (PLAINTEXT, "plaintext"),
  (SSL, "ssl"),
  (SASL_PLAIN, "sasl_plaintext"),
  (SASL_SSL, "sasl_ssl")
)

SMART_ENUM(SASLMechanismOption,
  (GSSAPI, "GSSAPI"),
  (PLAIN, "PLAIN")
)
}  // namespace kafka

class KafkaProcessorBase : public core::Processor {
 public:
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<0, 1>::createProperty("SSL Context Service")
        .withDescription("SSL Context Service Name")
        .withAllowedTypes({core::className<minifi::controllers::SSLContextService>()})
        .build();
  EXTENSIONAPI static constexpr auto SecurityProtocol = core::PropertyDefinitionBuilder<kafka::SecurityProtocolOption::length>::createProperty("Security Protocol")
        .withDescription("Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.")
        .withDefaultValue(toStringView(kafka::SecurityProtocolOption::PLAINTEXT))
        .withAllowedValues(kafka::SecurityProtocolOption::values)
        .isRequired(true)
        .build();
  EXTENSIONAPI static constexpr auto KerberosServiceName = core::PropertyDefinitionBuilder<>::createProperty("Kerberos Service Name")
        .withDescription("Kerberos Service Name")
        .build();
  EXTENSIONAPI static constexpr auto KerberosPrincipal = core::PropertyDefinitionBuilder<>::createProperty("Kerberos Principal")
        .withDescription("Kerberos Principal")
        .build();
  EXTENSIONAPI static constexpr auto KerberosKeytabPath = core::PropertyDefinitionBuilder<>::createProperty("Kerberos Keytab Path")
        .withDescription("The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.")
        .build();
  EXTENSIONAPI static constexpr auto SASLMechanism = core::PropertyDefinitionBuilder<kafka::SASLMechanismOption::length>::createProperty("SASL Mechanism")
        .withDescription("The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.")
        .withDefaultValue(toStringView(kafka::SASLMechanismOption::GSSAPI))
        .withAllowedValues(kafka::SASLMechanismOption::values)
        .isRequired(true)
        .build();
  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
        .withDescription("The username when the SASL Mechanism is sasl_plaintext")
        .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
        .withDescription("The password for the given username when the SASL Mechanism is sasl_plaintext")
        .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 8>{
      SSLContextService,
      SecurityProtocol,
      KerberosServiceName,
      KerberosPrincipal,
      KerberosKeytabPath,
      SASLMechanism,
      Username,
      Password
  };


  KafkaProcessorBase(std::string name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
      : core::Processor(std::move(name), uuid),
        logger_(logger) {
  }

 protected:
  virtual std::optional<utils::net::SslData> getSslData(core::ProcessContext& context) const;
  void setKafkaAuthenticationParameters(core::ProcessContext& context, gsl::not_null<rd_kafka_conf_t*> config);

  kafka::SecurityProtocolOption security_protocol_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
