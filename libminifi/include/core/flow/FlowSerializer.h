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
#include "core/ParameterContext.h"

namespace org::apache::nifi::minifi::core::flow {

class Overrides {
 public:
  Overrides& add(std::string_view property_name, std::string_view property_value);
  Overrides& addOptional(std::string_view property_name, std::string_view property_value);
  [[nodiscard]] std::optional<std::string> get(std::string_view property_name) const;
  [[nodiscard]] std::vector<std::pair<std::string, std::string>> getRequired() const;

 private:
  struct OverrideItem {
    std::string value;
    bool is_required;
  };
  std::unordered_map<std::string, OverrideItem> overrides_;
};

class FlowSerializer {
 public:
  FlowSerializer() = default;
  virtual ~FlowSerializer() = default;

  FlowSerializer(const FlowSerializer&) = delete;
  FlowSerializer& operator=(const FlowSerializer&) = delete;
  FlowSerializer(FlowSerializer&&) = delete;
  FlowSerializer& operator=(FlowSerializer&&) = delete;

  [[nodiscard]] virtual std::string serialize(const core::ProcessGroup& process_group, const FlowSchema& schema, const utils::crypto::EncryptionProvider& encryption_provider,
      const std::unordered_map<utils::Identifier, Overrides>& overrides, const std::unordered_map<std::string, gsl::not_null<std::unique_ptr<ParameterContext>>>& parameter_contexts) const = 0;
};

}  // namespace org::apache::nifi::minifi::core::flow
