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

#include <string>

#include "minifi-c.h"
#include "nonstd/expected.hpp"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "api/core/FlowFile.h"

namespace org::apache::nifi::minifi::api::core {

class ProcessContext {
 public:
  explicit ProcessContext(MinifiProcessContext* impl): impl_(impl) {}

  nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile* flow_file = nullptr) const;
  nonstd::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& property_reference, const FlowFile* flow_file = nullptr) const {
    return getProperty(property_reference.name, flow_file);
  }

  bool hasNonEmptyProperty(std::string_view name) const;

 private:
  MinifiProcessContext* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
