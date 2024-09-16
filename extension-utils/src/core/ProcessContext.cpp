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

#include "minifi-cpp/core/ProcessContext.h"
#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::core {

bool ProcessContext::getProperty(std::string_view name, detail::NotAFlowFile auto &value) const {
  if constexpr (std::is_base_of_v<TransformableValue, std::decay_t<decltype(value)>>) {
    std::string prop_str;
    if (!getProperty(std::string{name}, prop_str)) {
      return false;
    }
    value = std::decay_t<decltype(value)>(std::move(prop_str));
    return true;
  } else {
    return getProperty(name, value);
  }
}

}  // namespace org::apache::nifi::minifi::core
