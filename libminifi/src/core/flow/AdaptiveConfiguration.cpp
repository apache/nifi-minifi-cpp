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

#include "rapidjson/document.h"
#include "core/json/JsonNode.h"
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
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(payload.c_str(), payload.length());
  if (res) {
    flow::Node root{std::make_shared<JsonNode>(&doc)};
    if (root[FlowSchema::getDefault().flow_header]) {
      logger_->log_debug("Processing configuration as default json");
      return getRootFrom(root, FlowSchema::getDefault());
    } else {
      logger_->log_debug("Processing configuration as nifi flow json");
      return getRootFrom(root, FlowSchema::getNiFiFlowJson());
    }
  }
  logger_->log_debug("Could not parse configuration as json, trying yaml");

  try {
    YAML::Node rootYamlNode = YAML::Load(payload);
    flow::Node root{std::make_shared<YamlNode>(rootYamlNode)};
    return getRootFrom(root, FlowSchema::getDefault());
  } catch (const YAML::ParserException& ex) {
    logger_->log_error("Configuration file is not valid json: {} ({})", rapidjson::GetParseError_En(res.Code()), gsl::narrow<size_t>(res.Offset()));
    logger_->log_error("Configuration file is not valid yaml: {}", ex.what());
    throw;
  }
}

}  // namespace org::apache::nifi::minifi::core::flow
