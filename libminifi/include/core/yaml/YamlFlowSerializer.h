// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "yaml-cpp/yaml.h"
#include "core/flow/FlowSerializer.h"

namespace org::apache::nifi::minifi::core::yaml {

class YamlFlowSerializer : public core::flow::FlowSerializer {
 public:
  explicit YamlFlowSerializer(const YAML::Node& flow_definition_yaml) : flow_definition_yaml_(flow_definition_yaml) {}

  [[nodiscard]] std::string serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
      const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides,
      const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const override;

 private:
  static void addProviderCreatedParameterContexts(YAML::Node flow_definition_yaml, const core::flow::FlowSchema& schema,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts);
  void encryptSensitiveParameters(YAML::Node& flow_definition_yaml, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
    const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProcessorProperties(YAML::Node& flow_definition_yaml, const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema,
    const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveControllerServiceProperties(YAML::Node& flow_definition_yaml, const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema,
    const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProperties(YAML::Node property_yamls, const std::map<std::string, Property, std::less<>>& properties, const utils::crypto::EncryptionProvider& encryption_provider,
      const core::flow::Overrides& overrides) const;

  YAML::Node flow_definition_yaml_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<YamlFlowSerializer>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::core::yaml
