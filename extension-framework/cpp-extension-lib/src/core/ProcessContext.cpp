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

#include "api/core/ProcessContext.h"
#include "api/utils/minifi-c-utils.h"
#include "api/core/FlowFile.h"

namespace org::apache::nifi::minifi::api::core {

nonstd::expected<std::string, std::error_code> ProcessContext::getProperty(std::string_view name, const FlowFile* flow_file) const {
  std::optional<std::string> value;
  MinifiStatus status = MinifiProcessContextGetProperty(impl_, utils::toStringView(name), flow_file ? flow_file->get() : MINIFI_NULL,
    [] (void* data, MinifiStringView result) {
      (*static_cast<std::optional<std::string>*>(data)) = std::string(result.data, result.length);
    }, &value);

  if (!value) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return value.value();
}

bool ProcessContext::hasNonEmptyProperty(std::string_view name) const {
  return MinifiProcessContextHasNonEmptyProperty(impl_, utils::toStringView(name));
}

}  // namespace org::apache::nifi::minifi::api::core
