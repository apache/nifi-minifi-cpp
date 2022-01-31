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

void SupportedOperations::addProperty(SerializedResponseNode& properties, const std::string& operand, const std::unordered_map<std::string, std::string>& metadata) {
  SerializedResponseNode child;
  child.name = "properties";

  SerializedResponseNode operand_node;
  operand_node.name = "operand";
  operand_node.value = operand;

  SerializedResponseNode metadata_node;
  metadata_node.name = "metaData";
  metadata_node.array = true;

  for (const auto& [key, value] : metadata) {
    SerializedResponseNode metadata_child;
    metadata_child.name = "metaData";

    SerializedResponseNode key_node;
    key_node.name = "key";
    key_node.value = key;

    SerializedResponseNode value_node;
    value_node.name = "value";
    value_node.value = value;

    metadata_child.children.push_back(key_node);
    metadata_child.children.push_back(value_node);
    metadata_node.children.push_back(metadata_child);
  }

  child.children.push_back(operand_node);
  child.children.push_back(metadata_node);
  properties.children.push_back(child);
}

void SupportedOperations::fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation) const {
  switch (operation.value()) {
    case minifi::c2::Operation::DESCRIBE: {
      serializeProperty<minifi::c2::DescribeOperand>(properties);
      break;
    }
    case minifi::c2::Operation::UPDATE: {
      Metadata metadata;
      std::unordered_map<std::string, std::string> supported_config_update;
      for (const auto& config_property : minifi::Configuration::CONFIGURATION_PROPERTIES) {
        supported_config_update.emplace(config_property.name, config_property.validator->getName());
      }
      metadata.emplace("properties", supported_config_update);
      serializeProperty<minifi::c2::UpdateOperand>(properties, metadata);
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
    properties.array = true;

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
