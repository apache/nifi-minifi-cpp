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

#include "utils/ConfigurationUtils.h"

#include "minifi-cpp/properties/Configure.h"
#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::utils::configuration {

size_t getBufferSize(const Configure& configuration) {
  if (const auto buffer_size = configuration.get(Configure::nifi_default_internal_buffer_size); buffer_size && !buffer_size->empty()) {
    return parsing::parseIntegral<size_t>(*buffer_size) | utils::orThrow(fmt::format("Invalid value '{}' for {}", *buffer_size, Configure::nifi_default_internal_buffer_size));
  } else {
    return DEFAULT_BUFFER_SIZE;
  }
}

}  // namespace org::apache::nifi::minifi::utils::configuration
