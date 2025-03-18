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

#include <unordered_set>

#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

namespace org::apache::nifi::minifi::core::yaml {

void YamlFlowSerializer::addProviderCreatedParameterContexts(YAML::Node flow_definition_yaml, const core::flow::FlowSchema& schema,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const {
  std::vector<gsl::not_null<ParameterContext*>> provided_parameter_contexts;
  for (const auto& [parameter_context_name, parameter_context] : parameter_contexts) {
    if (!parameter_context->getParameterProvider().empty()) {
      provided_parameter_contexts.push_back(gsl::make_not_null(parameter_context.get()));
    }
  }

  if (provided_parameter_contexts.empty()) {
    return;
  }

  std::unordered_set<std::string> parameter_context_names;
  auto parameter_contexts_node = flow_definition_yaml[schema.parameter_contexts[0]];

  if (parameter_contexts_node.IsDefined() && parameter_contexts_node.IsSequence()) {
    for (const auto& parameter_context : parameter_contexts_node) {
      parameter_context_names.insert(parameter_context[schema.name[0]].as<std::string>());
    }
  } else {
    parameter_contexts_node = YAML::Node(YAML::NodeType::Sequence);
  }

  for (const auto& parameter_context : provided_parameter_contexts) {
    if (parameter_context_names.contains(parameter_context->getName())) {
      logger_->log_warn("Parameter context '{}' already exists in the flow definition, will not be updated!", parameter_context->getName());
      continue;
    }

    YAML::Node parameter_context_node;
    parameter_context_node[schema.identifier[0]] = std::string(parameter_context->getUUIDStr());
    parameter_context_node[schema.name[0]] = parameter_context->getName();
    parameter_context_node[schema.parameter_provider[0]] = parameter_context->getParameterProvider();

    YAML::Node parameters_node = YAML::Node(YAML::NodeType::Sequence);
    auto parameters = parameter_context->getParameters();
    for (const auto& [name, parameter] : parameters) {
      YAML::Node parameter_node;
      parameter_node[schema.name[0]] = name;
      parameter_node[schema.description[0]] = parameter.description;
      parameter_node[schema.sensitive[0]] = parameter.sensitive;
      parameter_node[schema.provided[0]] = parameter.provided;
      parameter_node[schema.value[0]] = parameter.value;

      parameters_node.push_back(parameter_node);
    }

    parameter_context_node[schema.parameters[0]] = parameters_node;
    parameter_contexts_node.push_back(parameter_context_node);
  }
}

void YamlFlowSerializer::encryptSensitiveProperties(YAML::Node property_yamls,
    const std::map<std::string, Property, std::less<>>& properties,
    const utils::crypto::EncryptionProvider& encryption_provider,
    const core::flow::Overrides& overrides) const {
  std::unordered_set<std::string> processed_property_names;

  for (auto kv : property_yamls) {
    auto name = kv.first.as<std::string>();
    if (!properties.contains(name)) {
      logger_->log_warn("Property {} found in flow definition does not exist!", name);
      continue;
    }
    if (properties.at(name).isSensitive()) {
      if (kv.second.IsSequence()) {
        for (auto property_item : kv.second) {
          const auto override_value = overrides.get(name);
          auto value = override_value ? *override_value : property_item["value"].as<std::string>();
          property_item["value"] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
        }
      } else {
        const auto override_value = overrides.get(name);
        auto value = override_value ? *override_value : kv.second.as<std::string>();
        property_yamls[name] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
      }
      processed_property_names.insert(name);
    }
  }

  for (const auto& [name, value] : overrides.getRequired()) {
    gsl_Expects(properties.contains(name) && properties.at(name).isSensitive());
    if (processed_property_names.contains(name)) { continue; }
    property_yamls[name] = utils::crypto::property_encryption::encrypt(value, encryption_provider);
  }
}

void YamlFlowSerializer::encryptSensitiveParameters(YAML::Node& flow_definition_yaml, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
    const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  for (auto parameter_context : flow_definition_yaml[schema.parameter_contexts[0]]) {
    for (auto parameter : parameter_context[schema.parameters[0]]) {
      bool is_sensitive = false;
      std::istringstream is(parameter[schema.sensitive[0]].Scalar());
      is >> std::boolalpha >> is_sensitive;
      if (!is_sensitive) {
        continue;
      }
      const auto parameter_context_id_str = parameter_context[schema.identifier[0]].Scalar();
      const auto parameter_context_id = utils::Identifier::parse(parameter_context_id_str);
      if (!parameter_context_id) {
        logger_->log_warn("Invalid parameter context ID found in the flow definition: {}", parameter_context_id_str);
        continue;
      }
      auto parameter_value = parameter[schema.value[0]].Scalar();
      if (overrides.contains(*parameter_context_id)) {
        const auto& override_values = overrides.at(*parameter_context_id);
        const auto parameter_name = parameter[schema.name[0]].Scalar();
        if (auto parameter_override_value = override_values.get(parameter_name)) {
          parameter_value = *parameter_override_value;
        }
      }
      parameter[schema.value[0]] = utils::crypto::property_encryption::encrypt(parameter_value, encryption_provider);
    }
  }
}

void YamlFlowSerializer::encryptSensitiveProcessorProperties(YAML::Node& flow_definition_yaml, const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema,
    const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
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
    const auto processor_overrides = overrides.contains(*processor_id) ? overrides.at(*processor_id) : core::flow::Overrides{};
    encryptSensitiveProperties(processor_yaml[schema.processor_properties[0]], processor->getSupportedProperties(), encryption_provider, processor_overrides);
  }
}

void YamlFlowSerializer::encryptSensitiveControllerServiceProperties(YAML::Node& flow_definition_yaml, const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema,
    const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  for (auto controller_service_yaml : flow_definition_yaml[schema.controller_services[0]]) {
    const auto controller_service_id_str = controller_service_yaml[schema.identifier[0]].Scalar();
    const auto controller_service_id = utils::Identifier::parse(controller_service_id_str);
    if (!controller_service_id) {
      logger_->log_warn("Invalid controller service ID found in the flow definition: {}", controller_service_id_str);
      continue;
    }
    const auto* const controller_service_node = process_group.findControllerService(controller_service_id_str);
    if (!controller_service_node) {
      logger_->log_warn("Controller service node {} not found in the flow definition", controller_service_id_str);
      continue;
    }
    const auto controller_service = controller_service_node->getControllerServiceImplementation();
    if (!controller_service) {
      logger_->log_warn("Controller service {} not found in the flow definition", controller_service_id_str);
      continue;
    }
    const auto controller_service_overrides = overrides.contains(*controller_service_id) ? overrides.at(*controller_service_id) : core::flow::Overrides{};
    encryptSensitiveProperties(controller_service_yaml[schema.controller_service_properties[0]], controller_service->getSupportedProperties(), encryption_provider, controller_service_overrides);
  }
}

std::string YamlFlowSerializer::serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
    const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const {
  gsl_Expects(schema.identifier.size() == 1 &&
      schema.processors.size() == 1 && schema.processor_properties.size() == 1 &&
      schema.controller_services.size() == 1 && schema.controller_service_properties.size() == 1);

  auto flow_definition_yaml = YAML::Clone(flow_definition_yaml_);

  addProviderCreatedParameterContexts(flow_definition_yaml, schema, parameter_contexts);
  encryptSensitiveParameters(flow_definition_yaml, schema, encryption_provider, overrides);
  encryptSensitiveProcessorProperties(flow_definition_yaml, process_group, schema, encryption_provider, overrides);
  encryptSensitiveControllerServiceProperties(flow_definition_yaml, process_group, schema, encryption_provider, overrides);

  return YAML::Dump(flow_definition_yaml) + '\n';
}

}  // namespace org::apache::nifi::minifi::core::yaml
