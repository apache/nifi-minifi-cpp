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

#include "core/yaml/YamlFlowSerializer.h"

#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

namespace org::apache::nifi::minifi::core::yaml {

void YamlFlowSerializer::encryptSensitiveProperties(YAML::Node property_yamls, const std::map<std::string, Property>& properties, const utils::crypto::EncryptionProvider& encryption_provider,
    std::unordered_map<std::string, std::string> component_overrides) const {
  for (auto kv : property_yamls) {
    auto name = kv.first.as<std::string>();
    if (!properties.contains(name)) {
      logger_->log_warn("Property {} found in flow definition does not exist!", name);
      continue;
    }
    if (properties.at(name).isSensitive()) {
      if (kv.second.IsSequence()) {
        for (auto property_item : kv.second) {
          auto value = component_overrides.contains(name) ? component_overrides.at(name) : property_item["value"].as<std::string>();
          property_item["value"] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
        }
      } else {
        auto value = component_overrides.contains(name) ? component_overrides.at(name) : kv.second.as<std::string>();
        property_yamls[name] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
      }
      component_overrides.erase(name);
    }
  }

  for (const auto& [name, value] : component_overrides) {
    gsl_Expects(properties.contains(name) && properties.at(name).isSensitive());
    property_yamls[name] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
  }
}

std::string YamlFlowSerializer::serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
    const std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>>& overrides) const {
  gsl_Expects(schema.identifier.size() == 1 &&
      schema.processors.size() == 1 && schema.processor_properties.size() == 1 &&
      schema.controller_services.size() == 1 && schema.controller_service_properties.size() == 1);

  auto flow_definition_yaml = YAML::Clone(flow_definition_yaml_);

  for (auto processor_yaml : flow_definition_yaml[schema.processors[0]]) {
    const auto processor_id = utils::Identifier::parse(processor_yaml[schema.identifier[0]].Scalar());
    if (!processor_id) {
      logger_->log_warn("Invalid processor ID found in the flow definition: {}", processor_yaml[schema.identifier[0]].Scalar());
      continue;
    }
    const auto* processor = process_group.findProcessorById(*processor_id);
    if (!processor) {
      logger_->log_warn("Processor {} not found in the flow definition", processor_id->to_string());
      continue;
    }
    const auto& processor_overrides = overrides.contains(*processor_id) ? overrides.at(*processor_id) : std::unordered_map<std::string, std::string>{};
    encryptSensitiveProperties(processor_yaml[schema.processor_properties[0]], processor->getProperties(), encryption_provider, processor_overrides);
  }

  for (auto controller_service_yaml : flow_definition_yaml[schema.controller_services[0]]) {
    const auto controller_service_id_str = controller_service_yaml[schema.identifier[0]].Scalar();
    const auto controller_service_id = utils::Identifier::parse(controller_service_id_str);
    if (!controller_service_id) {
      logger_->log_warn("Invalid controller service ID found in the flow definition: {}", controller_service_id_str);
      continue;
    }
    const auto controller_service_node = process_group.findControllerService(controller_service_id_str);
    if (!controller_service_node) {
      logger_->log_warn("Controller service node {} not found in the flow definition", controller_service_id_str);
      continue;
    }
    const auto controller_service = controller_service_node->getControllerServiceImplementation();
    if (!controller_service) {
      logger_->log_warn("Controller service {} not found in the flow definition", controller_service_id_str);
      continue;
    }
    const auto& controller_service_overrides = overrides.contains(*controller_service_id) ? overrides.at(*controller_service_id) : std::unordered_map<std::string, std::string>{};
    encryptSensitiveProperties(controller_service_yaml[schema.controller_service_properties[0]], controller_service->getProperties(), encryption_provider, controller_service_overrides);
  }

  return YAML::Dump(flow_definition_yaml) + '\n';
}

}  // namespace org::apache::nifi::minifi::core::yaml
