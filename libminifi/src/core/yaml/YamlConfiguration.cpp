// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>

#include "core/yaml/YamlConfiguration.h"
#include "core/yaml/YamlFlowSerializer.h"
#include "core/yaml/YamlNode.h"
#include "Defaults.h"
#include "utils/RegexUtils.h"
#include "yaml-cpp/yaml.h"

namespace org::apache::nifi::minifi::core {

YamlConfiguration::YamlConfiguration(ConfigurationContext ctx)
    : StructuredConfiguration(([&] {
          if (!ctx.path) {
            ctx.path = DEFAULT_NIFI_CONFIG_YML;
          }
          return std::move(ctx);
        })(),
        logging::LoggerFactory<YamlConfiguration>::getLogger()) {}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getRootFromPayload(const std::string &yamlConfigPayload) {
  try {
    YAML::Node rootYamlNode = YAML::Load(yamlConfigPayload);
    flow::Node root{std::make_shared<YamlNode>(rootYamlNode)};
    flow_serializer_ = std::make_unique<core::yaml::YamlFlowSerializer>(rootYamlNode);
    return getRootFrom(root, flow::FlowSchema::getDefault());
  } catch (const YAML::ParserException &pe) {
    logger_->log_error("Configuration is not valid yaml: {}", pe.what());
    throw;
  }
}

}  // namespace org::apache::nifi::minifi::core
