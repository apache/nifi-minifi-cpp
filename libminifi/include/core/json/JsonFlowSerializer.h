// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "yaml-cpp/yaml.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "core/flow/FlowSerializer.h"

namespace org::apache::nifi::minifi::core::json {

class JsonFlowSerializer : public core::flow::FlowSerializer {
 public:
  explicit JsonFlowSerializer(rapidjson::Document document) : flow_definition_json_(std::move(document)) {}

  [[nodiscard]] std::string serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema,
      const utils::crypto::EncryptionProvider& encryption_provider,
      const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides,
      const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const override;

 private:
  static void addProviderCreatedParameterContexts(rapidjson::Value& flow_definition_json, rapidjson::Document::AllocatorType& alloc, const core::flow::FlowSchema& schema,
    const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts);
  void encryptSensitiveParameters(rapidjson::Value& flow_definition_json, rapidjson::Document::AllocatorType& alloc, const core::flow::FlowSchema& schema,
      const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProcessorProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveControllerServiceProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProperties(rapidjson::Value& property_jsons, rapidjson::Document::AllocatorType& alloc,
      const std::map<std::string, Property, std::less<>>& properties, const utils::crypto::EncryptionProvider& encryption_provider,
      const core::flow::Overrides& overrides) const;

  rapidjson::Document flow_definition_json_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<JsonFlowSerializer>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::core::json
