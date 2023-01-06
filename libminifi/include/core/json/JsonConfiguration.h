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

#include "core/FlowConfiguration.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessorConfig.h"
#include "Exception.h"
#include "io/StreamFactory.h"
#include "io/validation.h"
#include "sitetosite/SiteToSite.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileSystem.h"
#include "core/flow/StructuredConfiguration.h"

namespace org::apache::nifi::minifi::core {

class JsonConfiguration : public flow::StructuredConfiguration {
 public:
  explicit JsonConfiguration(ConfigurationContext ctx);

  ~JsonConfiguration() override = default;

  std::unique_ptr<core::ProcessGroup> getRoot() override;

  std::unique_ptr<core::ProcessGroup> getRootFromPayload(const std::string &json_config) override;
};

}  // namespace org::apache::nifi::minifi::core