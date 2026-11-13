// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "api/core/ProcessorImpl.h"
#include "api/utils/Export.h"
#include "api/utils/Ssl.h"
#include "core/PropertyDefinitionBuilder.h"
#include "rdkafka_utils.h"
#include "utils/Enum.h"
#include "minifi-api.h"

namespace org::apache::nifi::minifi::processors {

namespace kafka {
enum class SecurityProtocolOption { plaintext, ssl, sasl_plaintext, sasl_ssl };

enum class SASLMechanismOption { GSSAPI, PLAIN };
}  // namespace kafka

class KafkaProcessorBase : public api::core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr auto SSLContextService =
      core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
          .withDescription("SSL Context Service Name")
          .withAllowedType<MINIFI_SSL_CONTEXT_SERVICE_INTERFACE_PROPERTY_TYPE>()
          .build();
  EXTENSIONAPI static constexpr auto SecurityProtocol =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<kafka::SecurityProtocolOption>()>::createProperty(
          "Security Protocol")
          .withDescription(
              "Protocol used to communicate with brokers. Corresponds to "
              "Kafka's 'security.protocol' property.")
          .withDefaultValue(magic_enum::enum_name(kafka::SecurityProtocolOption::plaintext))
          .withAllowedValues(magic_enum::enum_names<kafka::SecurityProtocolOption>())
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto KerberosServiceName =
      core::PropertyDefinitionBuilder<>::createProperty("Kerberos Service Name")
          .withDescription("Kerberos Service Name")
          .build();
  EXTENSIONAPI static constexpr auto KerberosPrincipal =
      core::PropertyDefinitionBuilder<>::createProperty("Kerberos Principal").withDescription("Kerberos Principal").build();
  EXTENSIONAPI static constexpr auto KerberosKeytabPath =
      core::PropertyDefinitionBuilder<>::createProperty("Kerberos Keytab Path")
          .withDescription(
              "The path to the location on the local filesystem where the "
              "kerberos keytab is located. Read permission on the file is "
              "required.")
          .build();
  EXTENSIONAPI static constexpr auto SASLMechanism =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<kafka::SASLMechanismOption>()>::createProperty("SASL Mechanism")
          .withDescription(
              "The SASL mechanism to use for authentication. Corresponds to "
              "Kafka's 'sasl.mechanism' property.")
          .withDefaultValue(magic_enum::enum_name(kafka::SASLMechanismOption::GSSAPI))
          .withAllowedValues(magic_enum::enum_names<kafka::SASLMechanismOption>())
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto Username =
      core::PropertyDefinitionBuilder<>::createProperty("Username")
          .withDescription("The username when the SASL Mechanism is sasl_plaintext")
          .build();
  EXTENSIONAPI static constexpr auto Password =
      core::PropertyDefinitionBuilder<>::createProperty("Password")
          .withDescription(
              "The password for the given username when the SASL Mechanism is "
              "sasl_plaintext")
          .isSensitive(true)
          .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({SSLContextService,
      SecurityProtocol, KerberosServiceName, KerberosPrincipal, KerberosKeytabPath, SASLMechanism, Username, Password});

  explicit KafkaProcessorBase(core::ProcessorMetadata metadata);

  KafkaProcessorBase(const KafkaProcessorBase&) = delete;
  KafkaProcessorBase(KafkaProcessorBase&&) = delete;
  KafkaProcessorBase& operator=(const KafkaProcessorBase&) = delete;
  KafkaProcessorBase& operator=(KafkaProcessorBase&&) = delete;
  ~KafkaProcessorBase() override = default;

 protected:
  virtual std::optional<api::utils::net::SslData> getSslData(api::core::ProcessContext& context) const;
  void setKafkaAuthenticationParameters(api::core::ProcessContext& context, gsl::not_null<rd_kafka_conf_t*> config);

  kafka::SecurityProtocolOption security_protocol_{};
  utils::KafkaOpaque kafka_opaque_;
};

}  // namespace org::apache::nifi::minifi::processors
