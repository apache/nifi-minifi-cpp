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

#include "core/flow/AdaptiveConfiguration.h"

#include <core/flow/FlowMigrator.h>

#include "core/json/JsonFlowSerializer.h"
#include "core/json/JsonNode.h"
#include "core/yaml/YamlFlowSerializer.h"
#include "core/yaml/YamlNode.h"
#include "yaml-cpp/yaml.h"
#include "utils/file/FileUtils.h"
#include "Defaults.h"
#include "rapidjson/error/en.h"

namespace org::apache::nifi::minifi::core::flow {

AdaptiveConfiguration::AdaptiveConfiguration(ConfigurationContext ctx)
    : StructuredConfiguration(([&] {
        if (!ctx.path) {
          if (utils::file::exists(DEFAULT_NIFI_CONFIG_JSON)) {
            ctx.path = DEFAULT_NIFI_CONFIG_JSON;
          } else {
            ctx.path = DEFAULT_NIFI_CONFIG_YML;
          }
        }
        return std::move(ctx);
      })(),
      logging::LoggerFactory<AdaptiveConfiguration>::getLogger()) {}

std::unique_ptr<core::ProcessGroup> AdaptiveConfiguration::getRootFromPayload(const std::string &payload) {
  rapidjson::Document root_json_node;
  const rapidjson::ParseResult json_parse_result = root_json_node.Parse(payload.c_str(), payload.length());
  if (json_parse_result) {
    Node root{std::make_shared<JsonNode>(&root_json_node, root_json_node.GetAllocator())};
    FlowSchema schema;
    if (root[FlowSchema::getDefault().flow_header]) {
      logger_->log_debug("Processing configuration as default json");
      schema = FlowSchema::getDefault();
    } else {
      logger_->log_debug("Processing configuration as nifi flow json");
      schema = FlowSchema::getNiFiFlowJson();
    }
    migrate(root, schema);
    const auto at_exit = gsl::finally([&] { flow_serializer_ = std::make_unique<core::json::JsonFlowSerializer>(std::move(root_json_node)); });
    return getRootFrom(root, schema);
  }

  logger_->log_debug("Could not parse configuration as json, trying yaml");

  try {
    YAML::Node root_yaml_node = YAML::Load(payload);
    flow::Node flow_root{std::make_shared<YamlNode>(root_yaml_node)};
    migrate(flow_root, FlowSchema::getDefault());

    flow_serializer_ = std::make_unique<core::yaml::YamlFlowSerializer>(root_yaml_node);
    return getRootFrom(flow_root, FlowSchema::getDefault());
  } catch (const YAML::ParserException& ex) {
    logger_->log_error("Configuration file is not valid json: {} ({})", rapidjson::GetParseError_En(json_parse_result.Code()), gsl::narrow<size_t>(json_parse_result.Offset()));
    logger_->log_error("Configuration file is not valid yaml: {}", ex.what());
    throw;
  }
}

void AdaptiveConfiguration::setSensitivePropertiesEncryptor(utils::crypto::EncryptionProvider sensitive_values_encryptor) {
  sensitive_values_encryptor_ = std::move(sensitive_values_encryptor);
}

std::string AdaptiveConfiguration::serializeWithOverrides(const core::ProcessGroup& process_group, const std::unordered_map<utils::Identifier, core::flow::Overrides>& overrides) const {
  gsl_Expects(flow_serializer_);
  return flow_serializer_->serialize(process_group, schema_, sensitive_values_encryptor_, overrides);
}

}  // namespace org::apache::nifi::minifi::core::flow
