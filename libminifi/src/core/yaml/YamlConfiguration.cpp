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

#include <memory>
#include <vector>
#include <set>
#include <cinttypes>

#include "core/yaml/YamlConfiguration.h"
#include "core/state/Value.h"
#include "Defaults.h"
#include "utils/TimeUtil.h"
#include "yaml-cpp/yaml.h"
#include "core/yaml/YamlNode.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::core {

YamlConfiguration::YamlConfiguration(ConfigurationContext ctx)
    : StructuredConfiguration(([&] {
          if (!ctx.path) {
            ctx.path = DEFAULT_NIFI_CONFIG_YML;
          }
          return std::move(ctx);
        })(),
        logging::LoggerFactory<YamlConfiguration>::getLogger()) {}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getRoot() {
  if (!config_path_) {
    logger_->log_error("Cannot instantiate flow, no config file is set.");
    throw Exception(ExceptionType::FLOW_EXCEPTION, "No config file specified");
  }
  const auto configuration = filesystem_->read(config_path_.value());
  if (!configuration) {
  // non-existence of flow config file is not a dealbreaker, the caller might fetch it from network
  return nullptr;
  }
  try {
    YAML::Node rootYamlNode = YAML::Load(configuration.value());
    flow::Node root{std::make_shared<YamlNode>(rootYamlNode)};
    return getRootFrom(root);
  } catch(...) {
    logger_->log_error("Invalid yaml configuration file");
    throw;
  }
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getYamlRoot(std::istream &yamlConfigStream) {
  try {
    YAML::Node rootYamlNode = YAML::Load(yamlConfigStream);
    flow::Node root{std::make_shared<YamlNode>(rootYamlNode)};
    return getRootFrom(root);
  } catch (const YAML::ParserException &pe) {
    logger_->log_error(pe.what());
    throw;
  }
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getRootFromPayload(const std::string &yamlConfigPayload) {
  try {
    YAML::Node rootYamlNode = YAML::Load(yamlConfigPayload);
    flow::Node root{std::make_shared<YamlNode>(rootYamlNode)};
    return getRootFrom(root);
  } catch (const YAML::ParserException &pe) {
    logger_->log_error(pe.what());
    throw;
  }
}

}  // namespace org::apache::nifi::minifi::core
