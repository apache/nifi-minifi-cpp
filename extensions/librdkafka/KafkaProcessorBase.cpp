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
#include "KafkaProcessorBase.h"

#include "rdkafka_utils.h"
#include "utils/ProcessorConfigUtils.h"
#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::processors {

std::optional<utils::net::SslData> KafkaProcessorBase::getSslData(core::ProcessContext& context) const {
  return utils::net::getSslData(context, SSLContextService, logger_);
}

void KafkaProcessorBase::setKafkaAuthenticationParameters(core::ProcessContext& context, gsl::not_null<rd_kafka_conf_t*> config) {
  security_protocol_ = utils::getRequiredPropertyOrThrow<kafka::SecurityProtocolOption>(context, std::string{SecurityProtocol.name});
  utils::setKafkaConfigurationField(*config, "security.protocol", security_protocol_.toString());
  logger_->log_debug("Kafka security.protocol [%s]", security_protocol_.toString());
  if (security_protocol_ == kafka::SecurityProtocolOption::SSL || security_protocol_ == kafka::SecurityProtocolOption::SASL_SSL) {
    auto ssl_data = getSslData(context);
    if (ssl_data) {
      if (ssl_data->ca_loc.empty() && ssl_data->cert_loc.empty() && ssl_data->key_loc.empty() && ssl_data->key_pw.empty()) {
        logger_->log_warn("Security protocol is set to %s, but no valid security parameters are set in the properties or in the SSL Context Service.", security_protocol_.toString());
      } else {
        utils::setKafkaConfigurationField(*config, "ssl.ca.location", ssl_data->ca_loc.string());
        logger_->log_debug("Kafka ssl.ca.location [%s]", ssl_data->ca_loc.string());
        utils::setKafkaConfigurationField(*config, "ssl.certificate.location", ssl_data->cert_loc.string());
        logger_->log_debug("Kafka ssl.certificate.location [%s]", ssl_data->cert_loc.string());
        utils::setKafkaConfigurationField(*config, "ssl.key.location", ssl_data->key_loc.string());
        logger_->log_debug("Kafka ssl.key.location [%s]", ssl_data->key_loc.string());
        utils::setKafkaConfigurationField(*config, "ssl.key.password", ssl_data->key_pw);
        logger_->log_debug("Kafka ssl.key.password was set");
      }
    }
  }

  auto sasl_mechanism = utils::getRequiredPropertyOrThrow<kafka::SASLMechanismOption>(context, std::string{SASLMechanism.name});
  utils::setKafkaConfigurationField(*config, "sasl.mechanism", sasl_mechanism.toString());
  logger_->log_debug("Kafka sasl.mechanism [%s]", sasl_mechanism.toString());

  auto setKafkaConfigIfNotEmpty = [this, &context, config](const core::PropertyReference& property, const std::string& kafka_config_name, bool log_value = true) {
    std::string value;
    if (context.getProperty(property, value) && !value.empty()) {
      utils::setKafkaConfigurationField(*config, kafka_config_name, value);
      if (log_value) {
        logger_->log_debug("Kafka %s [%s]", kafka_config_name, value);
      } else {
        logger_->log_debug("Kafka %s was set", kafka_config_name);
      }
    }
  };

  setKafkaConfigIfNotEmpty(KerberosServiceName, "sasl.kerberos.service.name");
  setKafkaConfigIfNotEmpty(KerberosPrincipal, "sasl.kerberos.principal");
  setKafkaConfigIfNotEmpty(KerberosKeytabPath, "sasl.kerberos.keytab");
  setKafkaConfigIfNotEmpty(Username, "sasl.username");
  setKafkaConfigIfNotEmpty(Password, "sasl.password", false);
}

}  // namespace org::apache::nifi::minifi::processors
