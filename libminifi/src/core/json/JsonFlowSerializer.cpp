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

#include "core/json/JsonFlowSerializer.h"

#include <unordered_set>

#include "rapidjson/prettywriter.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#endif

namespace org::apache::nifi::minifi::core::json {

namespace {
rapidjson::Value& getMember(rapidjson::Value& node, const std::string& member_name) {
  if (member_name == ".") {
    return node;
  } else {
    return node[member_name];
  }
}
}

void JsonFlowSerializer::encryptSensitiveProperties(rapidjson::Value& property_jsons, rapidjson::Document::AllocatorType& alloc,
    const std::map<std::string, Property>& properties, const utils::crypto::EncryptionProvider& encryption_provider,
    const core::flow::Overrides& overrides) const {
  std::unordered_set<std::string> processed_property_names;

  for (auto &property : property_jsons.GetObject()) {
    const std::string name{property.name.GetString(), property.name.GetStringLength()};
    if (!properties.contains(name)) {
      logger_->log_warn("Property {} found in flow definition does not exist!", name);
      continue;
    }
    if (properties.at(name).isSensitive()) {
      auto& value = property.value;
      const auto override_value = overrides.get(name);
      const std::string_view value_sv = override_value ? *override_value : std::string_view{value.GetString(), value.GetStringLength()};
      const std::string encrypted_value = utils::crypto::property_encryption::encrypt(value_sv, encryption_provider);
      value.SetString(encrypted_value.c_str(), gsl::narrow<rapidjson::SizeType>(encrypted_value.size()), alloc);
    }
    processed_property_names.insert(name);
  }

  for (const auto& [name, value] : overrides.getRequired()) {
    gsl_Expects(properties.contains(name) && properties.at(name).isSensitive());
    if (processed_property_names.contains(name)) { continue; }
    const std::string encrypted_value = utils::crypto::property_encryption::encrypt(value, encryption_provider);
    property_jsons.AddMember(rapidjson::Value(name, alloc), rapidjson::Value(encrypted_value, alloc), alloc);
  }
}

void JsonFlowSerializer::encryptSensitiveParameters(rapidjson::Value& flow_definition_json, rapidjson::Document::AllocatorType& alloc, const core::flow::FlowSchema& schema,
    const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  if (!flow_definition_json.HasMember(schema.parameter_contexts[0])) {
    return;
  }

  auto parameter_contexts = getMember(flow_definition_json, schema.parameter_contexts[0]).GetArray();
  for (auto& parameter_context : parameter_contexts) {
    for (auto& parameter : getMember(parameter_context, schema.parameters[0]).GetArray()) {
      bool is_sensitive = getMember(parameter, schema.sensitive[0]).GetBool();
      if (!is_sensitive) {
        continue;
      }
      const std::string parameter_context_id_str{getMember(parameter_context, schema.identifier[0]).GetString(), getMember(parameter_context, schema.identifier[0]).GetStringLength()};
      const auto parameter_context_id = utils::Identifier::parse(parameter_context_id_str);
      if (!parameter_context_id) {
        logger_->log_warn("Invalid parameter context ID found in the flow definition: {}", parameter_context_id_str);
        continue;
      }

      std::string parameter_value{getMember(parameter, schema.value[0]).GetString(), getMember(parameter, schema.value[0]).GetStringLength()};
      if (overrides.contains(*parameter_context_id)) {
        const auto& override_values = overrides.at(*parameter_context_id);
        std::string parameter_name{getMember(parameter, schema.name[0]).GetString(), getMember(parameter, schema.name[0]).GetStringLength()};
        if (auto parameter_override_value = override_values.get(parameter_name)) {
          parameter_value = *parameter_override_value;
        }
      }
      parameter_value = utils::crypto::property_encryption::encrypt(parameter_value, encryption_provider);
      getMember(parameter, schema.value[0]).SetString(parameter_value.c_str(), gsl::narrow<rapidjson::SizeType>(parameter_value.size()), alloc);
    }
  }
}

void JsonFlowSerializer::encryptSensitiveProcessorProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  auto processors = getMember(root_group, schema.processors[0]).GetArray();
  for (auto &processor_json : processors) {
    const std::string processor_id_str{getMember(processor_json, schema.identifier[0]).GetString(), getMember(processor_json, schema.identifier[0]).GetStringLength()};
    const auto processor_id = utils::Identifier::parse(processor_id_str);
    if (!processor_id) {
      logger_->log_warn("Invalid processor ID found in the flow definition: {}", processor_id_str);
      continue;
    }
    const auto processor = process_group.findProcessorById(*processor_id);
    if (!processor) {
      logger_->log_warn("Processor {} not found in the flow definition", processor_id->to_string());
      continue;
    }
    const auto processor_overrides = overrides.contains(*processor_id) ? overrides.at(*processor_id) : core::flow::Overrides{};
    encryptSensitiveProperties(getMember(processor_json, schema.processor_properties[0]), alloc, processor->getSupportedProperties(), encryption_provider,
        processor_overrides);
  }
}

void JsonFlowSerializer::encryptSensitiveControllerServiceProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  auto controller_services = getMember(root_group, schema.controller_services[0]).GetArray();
  for (auto &controller_service_json : controller_services) {
    const std::string controller_service_id_str{getMember(controller_service_json, schema.identifier[0]).GetString(), getMember(controller_service_json, schema.identifier[0]).GetStringLength()};
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
    encryptSensitiveProperties(getMember(controller_service_json, schema.controller_service_properties[0]), alloc, controller_service->getSupportedProperties(), encryption_provider,
        controller_service_overrides);
  }
}

std::string JsonFlowSerializer::serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
    const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  gsl_Expects(schema.root_group.size() == 1 && schema.identifier.size() == 1 &&
      schema.processors.size() == 1 && schema.processor_properties.size() == 1 &&
      schema.controller_services.size() == 1 && schema.controller_service_properties.size() == 1);

  rapidjson::Document doc;
  auto alloc = doc.GetAllocator();
  rapidjson::Value flow_definition_json;
  flow_definition_json.CopyFrom(flow_definition_json_, alloc);
  auto& root_group = getMember(flow_definition_json, schema.root_group[0]);

  encryptSensitiveParameters(flow_definition_json, alloc, schema, encryption_provider, overrides);
  encryptSensitiveProcessorProperties(root_group, alloc, process_group, schema, encryption_provider, overrides);
  encryptSensitiveControllerServiceProperties(root_group, alloc, process_group, schema, encryption_provider, overrides);

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{buffer};
  flow_definition_json.Accept(writer);
  return std::string(buffer.GetString(), buffer.GetSize()) + '\n';
}

}  // namespace org::apache::nifi::minifi::core::json

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
