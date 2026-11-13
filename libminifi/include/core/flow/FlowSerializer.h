// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
