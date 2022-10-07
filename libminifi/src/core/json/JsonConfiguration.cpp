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
#include <variant>

#include "core/json/JsonConfiguration.h"
#include "core/json/JsonNode.h"
#include "core/state/Value.h"
#include "Defaults.h"
#include "utils/TimeUtil.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::core {

namespace {

}  // namespace


JsonConfiguration::JsonConfiguration(ConfigurationContext ctx)
    : StructuredConfiguration(([&] {
                                if (!ctx.path) {
                                  ctx.path = DEFAULT_NIFI_CONFIG_JSON;
                                }
                                return std::move(ctx);
                              })(),
                              logging::LoggerFactory<JsonConfiguration>::getLogger()) {}

std::unique_ptr<core::ProcessGroup> JsonConfiguration::getRoot() {
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
    rapidjson::Document doc;
    rapidjson::ParseResult res = doc.Parse(configuration->c_str(), configuration->length());
    if (!res) {
      throw std::runtime_error("Could not parse json file");
    }
    flow::Node root{std::make_shared<JsonNode>(&doc)};
    return getRootFrom(root);
  } catch(...) {
    logger_->log_error("Invalid json configuration file");
    throw;
  }
}

std::unique_ptr<core::ProcessGroup> JsonConfiguration::getRootFromPayload(const std::string &json_config) {
  try {
    rapidjson::Document doc;
    rapidjson::ParseResult res = doc.Parse(json_config.c_str(), json_config.length());
    if (!res) {
      throw std::runtime_error("Could not parse json file");
    }
    flow::Node root{std::make_shared<JsonNode>(&doc)};
    return getRootFrom(root);
  } catch (const std::runtime_error& err) {
    logger_->log_error(err.what());
    throw;
  }
}

}  // namespace org::apache::nifi::minifi::core
