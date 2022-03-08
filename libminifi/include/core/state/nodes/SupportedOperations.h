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
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>

#include "MetricsBase.h"
#include "c2/C2Payload.h"
#include "controllers/UpdatePolicyControllerService.h"

namespace org::apache::nifi::minifi::state::response {

class SupportedOperations : public DeviceInformation {
 public:
  SupportedOperations(const std::string &name, const utils::Identifier &uuid);
  explicit SupportedOperations(const std::string &name);

  std::string getName() const override;
  std::vector<SerializedResponseNode> serialize() override;
  void setStateMonitor(std::shared_ptr<state::StateMonitor> monitor) {
    monitor_ = std::move(monitor);
  }

  void setUpdatePolicyController(std::shared_ptr<const controllers::UpdatePolicyControllerService> update_policy_controller) {
    update_policy_controller_ = std::move(update_policy_controller);
  }

 private:
  using Metadata = std::unordered_map<std::string, std::vector<std::unordered_map<std::string, std::string>>>;

  template<typename T>
  static void serializeProperty(SerializedResponseNode& properties, const std::unordered_map<std::string, Metadata>& operand_with_metadata = {}) {
    for (const auto& operand_type: T::values()) {
      auto metadata_it = operand_with_metadata.find(operand_type);
      if (metadata_it != operand_with_metadata.end()) {
        addProperty(properties, operand_type, metadata_it->second);
      } else {
        addProperty(properties, operand_type);
      }
    }
  }

  static void addProperty(SerializedResponseNode& properties, const std::string& operand, const Metadata& metadata = {});
  void fillProperties(SerializedResponseNode& properties, minifi::c2::Operation operation) const;
  Metadata buildUpdatePropertiesMetadata() const;

  std::shared_ptr<state::StateMonitor> monitor_;
  std::shared_ptr<const controllers::UpdatePolicyControllerService> update_policy_controller_;
};

}  // namespace org::apache::nifi::minifi::state::response
