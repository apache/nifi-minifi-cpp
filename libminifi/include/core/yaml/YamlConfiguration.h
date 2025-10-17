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
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/FlowConfiguration.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/ProcessorConfig.h"
#include "minifi-cpp/Exception.h"
#include "io/validation.h"
#include "sitetosite/SiteToSite.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileSystem.h"
#include "core/flow/StructuredConfiguration.h"

class YamlConfigurationTestAccessor;

namespace org::apache::nifi::minifi::core {

class YamlConfiguration : public flow::StructuredConfiguration {
 public:
  explicit YamlConfiguration(ConfigurationContext ctx);

  ~YamlConfiguration() override = default;

  std::vector<std::string> getSupportedFormats() const override {
    return {"text/yml", "application/vnd.minifi-c2+yaml;version=2"};
  }

  /**
   * Returns a shared pointer to a ProcessGroup object containing the
   * flow configuration. The yamlConfigPayload argument must be
   * a payload for the raw YAML configuration.
   *
   * @param yamlConfigPayload an input payload for the raw YAML configuration
   *                           to be parsed and loaded into the flow
   *                           configuration tree
   * @return                 the root ProcessGroup node of the flow
   *                           configuration tree
   */
  std::unique_ptr<core::ProcessGroup> getRootFromPayload(const std::string &yamlConfigPayload) override;
};

}  // namespace org::apache::nifi::minifi::core
