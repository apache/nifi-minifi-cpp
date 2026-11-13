// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  SupportedOperations(std::string_view name, const utils::Identifier &uuid);
  explicit SupportedOperations(std::string_view name);

  MINIFIAPI static constexpr const char* Description = "Metric node that defines the supported C2 operations in the Agent Manifest.";

  std::string getName() const override;
  std::vector<SerializedResponseNode> serialize() override;
  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }

  void setUpdatePolicyController(controllers::UpdatePolicyControllerService* update_policy_controller) {
    update_policy_controller_ = update_policy_controller;
  }

  void setConfigurationReader(std::function<std::optional<std::string>(const std::string&)> configuration_reader) {
    configuration_reader_ = configuration_reader;
  }

 private:
  using Metadata = std::vector<SerializedResponseNode>;

  template<typename T>
  static void serializeProperty(SerializedResponseNode& properties, const std::unordered_map<std::string, Metadata>& operand_with_metadata = {}) {
    for (const auto& operand_type_view : magic_enum::enum_names<T>()) {
      std::string operand_type{operand_type_view};
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

  state::StateMonitor* monitor_ = nullptr;
  controllers::UpdatePolicyControllerService* update_policy_controller_ = nullptr;
  std::function<std::optional<std::string>(const std::string&)> configuration_reader_;
};

}  // namespace org::apache::nifi::minifi::state::response
