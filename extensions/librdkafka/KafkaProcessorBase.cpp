/**
 * @file KafkaProcessorBase.cpp
 * KafkaProcessorBase class implementation
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
#include "KafkaProcessorBase.h"

#include "rdkafka_utils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const core::Property KafkaProcessorBase::SecurityProtocol(
        core::PropertyBuilder::createProperty("Security Protocol")
        ->withDescription("Protocol used to communicate with brokers")
        ->withDefaultValue<std::string>(toString(SecurityProtocolOption::PLAINTEXT))
        ->withAllowableValues<std::string>(SecurityProtocolOption::values())
        ->isRequired(true)
        ->build());
const core::Property KafkaProcessorBase::SSLContextService(
    core::PropertyBuilder::createProperty("SSL Context Service")
        ->withDescription("SSL Context Service Name")
        ->asType<minifi::controllers::SSLContextService>()
        ->build());
const core::Property KafkaProcessorBase::KerberosServiceName("Kerberos Service Name", "Kerberos Service Name", "");
const core::Property KafkaProcessorBase::KerberosPrincipal("Kerberos Principal", "Keberos Principal", "");
const core::Property KafkaProcessorBase::KerberosKeytabPath("Kerberos Keytab Path",
                                                            "The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.", "");
const core::Property KafkaProcessorBase::SASLMechanism(
        core::PropertyBuilder::createProperty("SASL Mechanism")
        ->withDescription("The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.")
        ->withDefaultValue<std::string>(toString(SASLMechanismOption::GSSAPI))
        ->withAllowableValues<std::string>(SASLMechanismOption::values())
        ->isRequired(true)
        ->build());
const core::Property KafkaProcessorBase::Username(
    core::PropertyBuilder::createProperty("Username")
        ->withDescription("The username when the SASL Mechanism is sasl_plaintext")
        ->build());
const core::Property KafkaProcessorBase::Password(
    core::PropertyBuilder::createProperty("Password")
        ->withDescription("The password for the given username when the SASL Mechanism is sasl_plaintext")
        ->build());

std::optional<utils::SSL_data> KafkaProcessorBase::getSslData(core::ProcessContext* context) const {
  utils::SSL_data ssl_data;

  std::string ssl_service_name;
  if (context->getProperty(SSLContextService.getName(), ssl_service_name) && !ssl_service_name.empty()) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(ssl_service_name);
    if (service) {
      auto ssl_service = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
      ssl_data.ca_loc = ssl_service->getCACertificate();
      ssl_data.cert_loc = ssl_service->getCertificateFile();
      ssl_data.key_loc = ssl_service->getPrivateKeyFile();
      ssl_data.key_pw = ssl_service->getPassphrase();
    } else {
      logger_->log_warn("SSL Context Service property is set to '%s', but the controller service could not be found.", ssl_service_name);
      return std::nullopt;
    }
  } else if (security_protocol_ == SecurityProtocolOption::SSL || security_protocol_ == SecurityProtocolOption::SASL_SSL) {
    logger_->log_warn("Security protocol is set to %s, but no valid SSL Context Service property is set.", security_protocol_.toString());
    return std::nullopt;
  }

  return ssl_data;
}

void KafkaProcessorBase::setKafkaAuthenticationParameters(core::ProcessContext* context, rd_kafka_conf_t* config) {
  security_protocol_ = utils::getRequiredPropertyOrThrow<SecurityProtocolOption>(context, SecurityProtocol.getName());
  utils::setKafkaConfigurationField(*config, "security.protocol", security_protocol_.toString());
  logger_->log_debug("Kafka security.protocol [%s]", security_protocol_.toString());
  if (security_protocol_ == SecurityProtocolOption::SSL || security_protocol_ == SecurityProtocolOption::SASL_SSL) {
    auto ssl_data = getSslData(context);
    if (ssl_data) {
      utils::setKafkaConfigurationField(*config, "ssl.ca.location", ssl_data->ca_loc);
      logger_->log_debug("Kafka ssl.ca.location [%s]", ssl_data->ca_loc);
      utils::setKafkaConfigurationField(*config, "ssl.certificate.location", ssl_data->cert_loc);
      logger_->log_debug("Kafka ssl.certificate.location [%s]", ssl_data->cert_loc);
      utils::setKafkaConfigurationField(*config, "ssl.key.location", ssl_data->key_loc);
      logger_->log_debug("Kafka ssl.key.location [%s]", ssl_data->key_loc);
      utils::setKafkaConfigurationField(*config, "ssl.key.password", ssl_data->key_pw);
      logger_->log_debug("Kafka ssl.key.password was set");

      if (ssl_data->ca_loc.empty() && ssl_data->cert_loc.empty() && ssl_data->key_loc.empty() && ssl_data->key_pw.empty()) {
        logger_->log_warn("Security protocol is set to %s, but no valid security parameters are set in the properties or in the SSL Context Service.", security_protocol_.toString());
      }
    }
  }

  auto sasl_mechanism = utils::getRequiredPropertyOrThrow<SASLMechanismOption>(context, SASLMechanism.getName());
  utils::setKafkaConfigurationField(*config, "sasl.mechanism", sasl_mechanism.toString());
  logger_->log_debug("Kafka sasl.mechanism [%s]", sasl_mechanism.toString());

  std::string value;
  if (context->getProperty(KerberosServiceName.getName(), value) && !value.empty()) {
    utils::setKafkaConfigurationField(*config, "sasl.kerberos.service.name", value);
    logger_->log_debug("Kafka sasl.kerberos.service.name [%s]", value);
  }

  value = "";
  if (context->getProperty(KerberosPrincipal.getName(), value) && !value.empty()) {
    utils::setKafkaConfigurationField(*config, "sasl.kerberos.principal", value);
    logger_->log_debug("Kafka sasl.kerberos.principal [%s]", value);
  }

  value = "";
  if (context->getProperty(KerberosKeytabPath.getName(), value) && !value.empty()) {
    utils::setKafkaConfigurationField(*config, "sasl.kerberos.keytab", value);
    logger_->log_debug("Kafka sasl.kerberos.keytab [%s]", value);
  }

  value = "";
  if (context->getProperty(Username.getName(), value) && !value.empty()) {
    utils::setKafkaConfigurationField(*config, "sasl.username", value);
    logger_->log_debug("Kafka sasl.username [%s]", value);
  }

  value = "";
  if (context->getProperty(Password.getName(), value) && !value.empty()) {
    utils::setKafkaConfigurationField(*config, "sasl.password", value);
    logger_->log_debug("Kafka sasl.password was set");
  }
}

}  // namespace org::apache::nifi::minifi::processors
