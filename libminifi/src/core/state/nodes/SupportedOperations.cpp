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

#include "core/state/nodes/SupportedOperations.h"
#include "core/Resource.h"
#include "range/v3/algorithm/contains.hpp"
#include "range/v3/view/filter.hpp"
#include "range/v3/view/view.hpp"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::state::response {

SupportedOperations::SupportedOperations(std::string_view name, const utils::Identifier &uuid)
    : DeviceInformation(name, uuid) {
  setArray(true);
}

SupportedOperations::SupportedOperations(std::string_view name)
    : DeviceInformation(name) {
  setArray(true);
}

std::string SupportedOperations::getName() const {
  return "supportedOperations";
}

void SupportedOperations::addProperty(SerializedResponseNode& properties, const std::string& operand, const Metadata& metadata) {
  SerializedResponseNode operand_node{.name = operand, .keep_empty = true};

  for (const auto& [key, value_array] : metadata) {
    SerializedResponseNode metadata_item{.name = key, .array = true};
    for (const auto& value_object : value_array) {
      SerializedResponseNode value_child;
      for (const auto& pair: value_object) {
        value_child.children.push_back({.name = pair.first, .value = pair.second});
      }
      metadata_item.children.push_back(value_child);
    }
    operand_node.children.push_back(metadata_item);
  }
  properties.children.push_back(operand_node);
}

SupportedOperations::Metadata SupportedOperations::buildUpdatePropertiesMetadata() const {
  std::vector<std::unordered_map<std::string, std::string>> supported_config_updates;
  auto sensitive_properties = Configuration::getSensitiveProperties(configuration_reader_);
  auto updatable_not_sensitive_configuration_properties = minifi::Configuration::CONFIGURATION_PROPERTIES | ranges::views::filter([&](const auto& configuration_property) {
    const auto& configuration_property_name = configuration_property.first;
    return !ranges::contains(sensitive_properties, configuration_property_name)
        && (!update_policy_controller_ || update_policy_controller_->canUpdate(std::string(configuration_property_name)));
  });

  for (const auto& [config_property_name, config_property_validator] : updatable_not_sensitive_configuration_properties) {
    std::unordered_map<std::string, std::string> property;
    property.emplace("propertyName", config_property_name);
    property.emplace("validator", config_property_validator->getEquivalentNifiStandardValidatorName().value_or("VALID"));
    if (configuration_reader_) {
      if (auto property_value = configuration_reader_(std::string(config_property_name))) {
        property.emplace("propertyValue", *property_value);
      }
    }
    supported_config_updates.push_back(property);
  }
  Metadata available_properties;
  available_properties.emplace("availableProperties", supported_config_updates);
  return available_properties;
}

void SupportedOperations::fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation) const {
  switch (operation) {
    case minifi::c2::Operation::describe: {
      serializeProperty<minifi::c2::DescribeOperand>(properties);
      break;
    }
    case minifi::c2::Operation::update: {
      std::unordered_map<std::string, Metadata> operand_with_metadata;
      operand_with_metadata.emplace("properties", buildUpdatePropertiesMetadata());
      serializeProperty<minifi::c2::UpdateOperand>(properties, operand_with_metadata);
      break;
    }
    case minifi::c2::Operation::transfer: {
      serializeProperty<minifi::c2::TransferOperand>(properties);
      break;
    }
    case minifi::c2::Operation::clear: {
      serializeProperty<minifi::c2::ClearOperand>(properties);
      break;
    }
    case minifi::c2::Operation::start:
    case minifi::c2::Operation::stop: {
      addProperty(properties, "c2");
      if (monitor_) {
        monitor_->executeOnAllComponents([&properties](StateController& component){
          addProperty(properties, component.getComponentUUID().to_string());
        });
      }
      break;
    }
    case minifi::c2::Operation::sync: {
      serializeProperty<minifi::c2::SyncOperand>(properties);
      break;
    }
    default:
      break;
  }
}

std::vector<SerializedResponseNode> SupportedOperations::serialize() {
  SerializedResponseNode supported_operation{.name = "supportedOperations", .array = true};

  for (const auto& operation : magic_enum::enum_names<minifi::c2::Operation>()) {
    SerializedResponseNode child{
      .name = "supportedOperations",
      .children = {
        {.name = "type", .value = std::string(operation)}
      }
    };

    SerializedResponseNode properties{.name = "properties"};
    fillProperties(properties, utils::enumCast<minifi::c2::Operation>(operation, true));
    child.children.push_back(properties);
    supported_operation.children.push_back(child);
  }

  return {supported_operation};
}

REGISTER_RESOURCE(SupportedOperations, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
