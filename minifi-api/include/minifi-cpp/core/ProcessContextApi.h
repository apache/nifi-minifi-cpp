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
#include <map>
#include "utils/expected.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"

namespace org::apache::nifi::minifi::core {

class ProcessContextApi {
 public:
  virtual nonstd::expected<std::string, std::error_code> getProperty(ProcessContext& context, std::string_view name, const FlowFile*) const = 0;
  virtual nonstd::expected<std::string, std::error_code> getDynamicProperty(ProcessContext& context, std::string_view name, const FlowFile*) const = 0;

  virtual std::map<std::string, std::string> getDynamicProperties(ProcessContext& context, const FlowFile*) const = 0;

  virtual ~ProcessContextApi() = default;
};

}  // namespace org::apache::nifi::minifi::core