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
#pragma once

#include <string>

#include "MetricsBase.h"
#include "c2/C2Payload.h"

namespace org::apache::nifi::minifi::state::response {

class SupportedOperations : public DeviceInformation {
 public:
  SupportedOperations(const std::string &name, const utils::Identifier &uuid);
  explicit SupportedOperations(const std::string &name);

  std::string getName() const override;
  std::vector<SerializedResponseNode> serialize() override;

 private:
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

  void fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation);
};

}  // org::apache::nifi::minifi::state::response
