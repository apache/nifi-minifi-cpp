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
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "core/flow/FlowSerializer.h"

namespace org::apache::nifi::minifi::core::json {

class JsonFlowSerializer : public core::flow::FlowSerializer {
 public:
  explicit JsonFlowSerializer(rapidjson::Document document) : flow_definition_json_(std::move(document)) {}

  [[nodiscard]] std::string serialize(const core::ProcessGroup& process_group, const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
      const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const override;

 private:
  void encryptSensitiveParameters(rapidjson::Value& flow_definition_json, rapidjson::Document::AllocatorType& alloc, const core::flow::FlowSchema& schema,
      const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProcessorProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveControllerServiceProperties(rapidjson::Value& root_group, rapidjson::Document::AllocatorType& alloc, const core::ProcessGroup& process_group,
    const core::flow::FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const;
  void encryptSensitiveProperties(rapidjson::Value& property_jsons, rapidjson::Document::AllocatorType& alloc,
      const std::map<std::string, Property>& properties, const utils::crypto::EncryptionProvider& encryption_provider,
      const core::flow::Overrides& overrides) const;

  rapidjson::Document flow_definition_json_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<JsonFlowSerializer>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::core::json
