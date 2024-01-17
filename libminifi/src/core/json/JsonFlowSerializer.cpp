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

#include "rapidjson/prettywriter.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#endif

namespace org::apache::nifi::minifi::core::json {

void JsonFlowSerializer::encryptSensitiveProperties(rapidjson::Value &property_jsons, rapidjson::Document::AllocatorType &alloc,
    const std::map<std::string, Property> &properties, const utils::crypto::EncryptionProvider &encryption_provider) const {
  for (auto &property : property_jsons.GetObject()) {
    const std::string name{property.name.GetString(), property.name.GetStringLength()};
    if (!properties.contains(name)) {
      logger_->log_warn("Property {} found in flow definition does not exist!", name);
      continue;
    }
    if (properties.at(name).isSensitive()) {
      auto& value = property.value;
      const std::string_view value_sv{value.GetString(), value.GetStringLength()};
      const std::string encrypted_value = utils::crypto::property_encryption::encrypt(value_sv, encryption_provider);
      value.SetString(encrypted_value.c_str(), encrypted_value.size(), alloc);
    }
  }
}

std::string JsonFlowSerializer::serialize(const core::ProcessGroup &process_group, const core::flow::FlowSchema &schema, const utils::crypto::EncryptionProvider &encryption_provider) const {
  gsl_Expects(schema.root_group.size() == 1 && schema.identifier.size() == 1 &&
      schema.processors.size() == 1 && schema.processor_properties.size() == 1 &&
      schema.controller_services.size() == 1 && schema.controller_service_properties.size() == 1);

  rapidjson::Document doc;
  auto alloc = doc.GetAllocator();
  rapidjson::Value flow_definition_json;
  flow_definition_json.CopyFrom(flow_definition_json_, alloc);
  auto &root_group = flow_definition_json[schema.root_group[0]];

  auto processors = root_group[schema.processors[0]].GetArray();
  for (auto &processor_json : processors) {
    const std::string processor_id_str{processor_json[schema.identifier[0]].GetString(), processor_json[schema.identifier[0]].GetStringLength()};
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
    encryptSensitiveProperties(processor_json[schema.processor_properties[0]], alloc, processor->getProperties(), encryption_provider);
  }

  auto controller_services = root_group[schema.controller_services[0]].GetArray();
  for (auto &controller_service_json : controller_services) {
    const std::string controller_service_id{controller_service_json[schema.identifier[0]].GetString(), controller_service_json[schema.identifier[0]].GetStringLength()};
    const auto controller_service_node = process_group.findControllerService(controller_service_id);
    if (!controller_service_node) {
      logger_->log_warn("Controller service node {} not found in the flow definition", controller_service_id);
      continue;
    }
    const auto controller_service = controller_service_node->getControllerServiceImplementation();
    if (!controller_service) {
      logger_->log_warn("Controller service {} not found in the flow definition", controller_service_id);
      continue;
    }
    encryptSensitiveProperties(controller_service_json[schema.controller_service_properties[0]], alloc, controller_service->getProperties(), encryption_provider);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{buffer};
  flow_definition_json.Accept(writer);
  return std::string(buffer.GetString(), buffer.GetSize()) + '\n';
}

}  // namespace org::apache::nifi::minifi::core::json

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
