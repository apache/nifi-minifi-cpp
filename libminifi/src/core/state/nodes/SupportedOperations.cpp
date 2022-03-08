/**
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

#include "core/state/nodes/SupportedOperations.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

SupportedOperations::SupportedOperations(const std::string &name, const utils::Identifier &uuid)
    : DeviceInformation(name, uuid) {
  setArray(true);
}

SupportedOperations::SupportedOperations(const std::string &name)
    : DeviceInformation(name) {
  setArray(true);
}

std::string SupportedOperations::getName() const {
  return "supportedOperations";
}

void SupportedOperations::addProperty(SerializedResponseNode& properties, const std::string& operand, const Metadata& metadata) {
  SerializedResponseNode operand_node;
  operand_node.name = operand;
  operand_node.keep_empty = true;
  operand_node.collapsible = false;

  for (const auto& [key, value_array] : metadata) {
    SerializedResponseNode metadata_item;
    metadata_item.collapsible = false;
    metadata_item.name = key;
    metadata_item.array = true;

    for (const auto& value_object : value_array) {
      SerializedResponseNode value_child;
      value_child.collapsible = false;
      for (const auto& pair: value_object) {
        SerializedResponseNode object_element;
        object_element.collapsible = false;
        object_element.name = pair.first;
        object_element.value = pair.second;
        value_child.children.push_back(object_element);
      }
      metadata_item.children.push_back(value_child);
    }

    operand_node.children.push_back(metadata_item);
  }

  properties.children.push_back(operand_node);
}

SupportedOperations::Metadata SupportedOperations::buildUpdatePropertiesMetadata() const {
  std::vector<std::unordered_map<std::string, std::string>> supported_config_updates;
  for (const auto& config_property : minifi::Configuration::CONFIGURATION_PROPERTIES) {
    if (!update_policy_controller_ ||
        (update_policy_controller_ && update_policy_controller_->canUpdate(std::string(config_property.name)))) {
      std::unordered_map<std::string, std::string> property;
      property.emplace("propertyName", config_property.name);
      property.emplace("validator", config_property.validator->getName());
      supported_config_updates.push_back(property);
    }
  }
  Metadata available_properties;
  available_properties.emplace("availableProperties", supported_config_updates);
  return available_properties;
}

void SupportedOperations::fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation) const {
  switch (operation.value()) {
    case minifi::c2::Operation::DESCRIBE: {
      serializeProperty<minifi::c2::DescribeOperand>(properties);
      break;
    }
    case minifi::c2::Operation::UPDATE: {
      std::unordered_map<std::string, Metadata> operand_with_metadata;
      operand_with_metadata.emplace("properties", buildUpdatePropertiesMetadata());
      serializeProperty<minifi::c2::UpdateOperand>(properties, operand_with_metadata);
      break;
    }
    case minifi::c2::Operation::TRANSFER: {
      serializeProperty<minifi::c2::TransferOperand>(properties);
      break;
    }
    case minifi::c2::Operation::CLEAR: {
      serializeProperty<minifi::c2::ClearOperand>(properties);
      break;
    }
    case minifi::c2::Operation::START:
    case minifi::c2::Operation::STOP: {
      addProperty(properties, "c2");
      if (monitor_) {
        for (const auto& component: monitor_->getAllComponents()) {
          addProperty(properties, component->getComponentName());
        }
      }
      break;
    }
    default:
      break;
  }
}

std::vector<SerializedResponseNode> SupportedOperations::serialize() {
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode supported_operation;
  supported_operation.name = "supportedOperations";
  supported_operation.array = true;

  for (const auto& operation : minifi::c2::Operation::values()) {
    SerializedResponseNode child;
    child.name = "supportedOperations";

    SerializedResponseNode operation_type;
    operation_type.name = "type";
    operation_type.value = operation;

    SerializedResponseNode properties;
    properties.name = "properties";
    properties.collapsible = false;

    fillProperties(properties, minifi::c2::Operation::parse(operation.c_str()));

    child.children.push_back(operation_type);
    child.children.push_back(properties);
    supported_operation.children.push_back(child);
  }

  serialized.push_back(supported_operation);
  return serialized;
}

REGISTER_RESOURCE(SupportedOperations, "Node part of an AST that defines the supported C2 operations in the Agent Manifest.");

}  // namespace org::apache::nifi::minifi::state::response
