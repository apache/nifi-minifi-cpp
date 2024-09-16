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
  void addProviderCreatedParameterContexts(YAML::Node flow_definition_yaml, const core::flow::FlowSchema& schema,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const;
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
