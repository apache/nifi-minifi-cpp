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
#include <unordered_map>

#include "core/flow/FlowSchema.h"
#include "core/ProcessGroup.h"
#include "utils/crypto/EncryptionProvider.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::core::flow {

class FlowSerializer {
 public:
  FlowSerializer() = default;
  virtual ~FlowSerializer() = default;

  FlowSerializer(const FlowSerializer&) = delete;
  FlowSerializer& operator=(const FlowSerializer&) = delete;
  FlowSerializer(FlowSerializer&&) = delete;
  FlowSerializer& operator=(FlowSerializer&&) = delete;

  [[nodiscard]] virtual std::string serialize(const core::ProcessGroup& process_group, const FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
      const std::unordered_map<utils::Identifier, std::unordered_map<std::string, std::string>>& overrides) const = 0;
};

}  // namespace org::apache::nifi::minifi::core::flow
