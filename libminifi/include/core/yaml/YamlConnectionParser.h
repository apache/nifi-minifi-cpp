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

#pragma once

#include <memory>
#include <string>

#include "core/ProcessGroup.h"
#include "core/logging/LoggerConfiguration.h"

#include "yaml-cpp/yaml.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace yaml {

class YamlConnectionParser {
 public:
  static constexpr const char* CONFIG_YAML_CONNECTIONS_KEY{ "Connections" };

  explicit YamlConnectionParser(const YAML::Node& connectionNode, const std::string& name, gsl::not_null<core::ProcessGroup*> parent, const std::shared_ptr<logging::Logger>& logger) :
      connectionNode_(connectionNode),
      name_(name),
      parent_(parent),
      logger_(logger) {}

  void configureConnectionSourceRelationshipsFromYaml(minifi::Connection& connection) const;
  [[nodiscard]] uint64_t getWorkQueueSizeFromYaml() const;
  [[nodiscard]] uint64_t getWorkQueueDataSizeFromYaml() const;
  [[nodiscard]] utils::Identifier getSourceUUIDFromYaml() const;
  [[nodiscard]] utils::Identifier getDestinationUUIDFromYaml() const;
  [[nodiscard]] std::chrono::milliseconds getFlowFileExpirationFromYaml() const;
  [[nodiscard]] bool getDropEmptyFromYaml() const;

 private:
  void addNewRelationshipToConnection(const std::string& relationship_name, minifi::Connection& connection) const;
  void addFunnelRelationshipToConnection(minifi::Connection& connection) const;

  const YAML::Node& connectionNode_;
  const std::string& name_;
  gsl::not_null<core::ProcessGroup*> parent_;
  const std::shared_ptr<logging::Logger> logger_;
};

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
