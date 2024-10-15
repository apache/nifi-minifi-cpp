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

#include "PublishKafkaMigrator.h"

#include "core/Resource.h"
#include "core/flow/FlowSchema.h"
#include "controllers/SSLContextService.h"
#include "../PublishKafka.h"


namespace org::apache::nifi::minifi::kafka::migration {

namespace {
constexpr std::string_view DEPRECATED_MESSAGE_KEY_FIELD = "Message Key Field";
constexpr std::string_view DEPRECATED_SECURITY_CA = "Security CA";
constexpr std::string_view DEPRECATED_SECURITY_CERT = "Security Cert";
constexpr std::string_view DEPRECATED_SECURITY_PRIVATE_KEY = "Security Private Key";
constexpr std::string_view DEPRECATED_SECURITY_PASS_PHRASE = "Security Pass Phrase";

void migrateKafkaPropertyToSSLContextService(
    const std::string_view deprecated_publish_kafka_property,
    const std::string_view ssl_context_service_property,
    core::flow::Node& publish_kafka_properties,
    core::flow::Node& ssl_controller_service_properties) {
  const auto security_ca = publish_kafka_properties.getMember(deprecated_publish_kafka_property);
  if (const auto security_ca_str = security_ca ? security_ca.getString() : std::nullopt) {
    ssl_controller_service_properties.addMember(ssl_context_service_property, *security_ca_str);
  }

  std::ignore = publish_kafka_properties.remove(deprecated_publish_kafka_property);
}
}  // namespace

void PublishKafkaMigrator::migrate(core::flow::Node& root_node, const core::flow::FlowSchema& schema) {
  auto publish_kafka_processors = getProcessors(root_node, schema, "PublishKafka");
  for (auto& publish_kafka_processor : publish_kafka_processors) {
    auto publish_kafka_properties = publish_kafka_processor[schema.processor_properties];
    if (publish_kafka_properties.remove(DEPRECATED_MESSAGE_KEY_FIELD)) {
      logger_->log_warn("Removed deprecated property \"{}\" from {}", DEPRECATED_MESSAGE_KEY_FIELD, *publish_kafka_processor[schema.identifier].getString());
    }
    if (publish_kafka_properties.contains(DEPRECATED_SECURITY_CA) ||
        publish_kafka_properties.contains(DEPRECATED_SECURITY_CERT) ||
        publish_kafka_properties.contains(DEPRECATED_SECURITY_PRIVATE_KEY) ||
        publish_kafka_properties.contains(DEPRECATED_SECURITY_PASS_PHRASE)) {
      std::string publish_kafka_id_str = publish_kafka_processor[schema.identifier].getString().value_or(std::string{utils::IdGenerator::getIdGenerator()->generate().to_string()});
      auto ssl_context_service_name = fmt::format("GeneratedSSLContextServiceFor_{}", publish_kafka_id_str);
      auto root_group = root_node[schema.root_group];
      auto controller_services = root_group[schema.controller_services];
      auto ssl_controller_service = *controller_services.pushBack();
      ssl_controller_service.addMember(schema.name[0], ssl_context_service_name);
      ssl_controller_service.addMember(schema.identifier[0], utils::IdGenerator::getIdGenerator()->generate().to_string().c_str());
      ssl_controller_service.addMember(schema.type[0], "SSLContextService");

      publish_kafka_properties.addMember(processors::PublishKafka::SSLContextService.name, ssl_context_service_name);
      auto ssl_controller_service_properties = ssl_controller_service.addObject(schema.controller_service_properties[0]);

      migrateKafkaPropertyToSSLContextService(DEPRECATED_SECURITY_CA, controllers::SSLContextService::CACertificate.name, publish_kafka_properties, *ssl_controller_service_properties);
      migrateKafkaPropertyToSSLContextService(DEPRECATED_SECURITY_CERT, controllers::SSLContextService::ClientCertificate.name, publish_kafka_properties, *ssl_controller_service_properties);
      migrateKafkaPropertyToSSLContextService(DEPRECATED_SECURITY_PRIVATE_KEY, controllers::SSLContextService::PrivateKey.name, publish_kafka_properties, *ssl_controller_service_properties);
      migrateKafkaPropertyToSSLContextService(DEPRECATED_SECURITY_PASS_PHRASE, controllers::SSLContextService::Passphrase.name, publish_kafka_properties, *ssl_controller_service_properties);

      logger_->log_warn("Removed deprecated Security Properties from {} and replaced them with SSLContextService", *publish_kafka_processor[schema.identifier].getString());
    }
  }
}

REGISTER_RESOURCE(PublishKafkaMigrator, FlowMigrator);
}  // namespace org::apache::nifi::minifi::kafka::migration
