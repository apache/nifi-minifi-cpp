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

template<typename T>
void serializeProperty(SerializedResponseNode& properties) {
  for (const auto& operand_type: T::values()) {
    SerializedResponseNode child;
    child.name = "properties";

    SerializedResponseNode operand;
    operand.name = "operand";
    operand.value = operand_type;
    child.children.push_back(operand);
    properties.children.push_back(child);
  }
}

void SupportedOperations::fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation) {
  switch(operation.value()) {
    case minifi::c2::Operation::DESCRIBE: {
      serializeProperty<minifi::c2::DescribeOperand>(properties);
      break;
    }
    case minifi::c2::Operation::UPDATE: {
      serializeProperty<minifi::c2::UpdateOperand>(properties);
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

}  // org::apache::nifi::minifi::state::response
