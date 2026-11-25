// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <map>
#include <string>
#include <system_error>

#include "minifi-api.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::core {

/// Property and controller service access shared by the schedule and trigger contexts
class PropertyContext {
 public:
  virtual ~PropertyContext() noexcept = default;

  PropertyContext() = default;
  PropertyContext(const PropertyContext&) = delete;
  PropertyContext(PropertyContext&&) = delete;
  PropertyContext& operator=(const PropertyContext&) = delete;
  PropertyContext& operator=(PropertyContext&&) = delete;

  [[nodiscard]] virtual std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop) const = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getDynamicProperties() const = 0;
  [[nodiscard]] virtual std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const = 0;
};

}  // namespace org::apache::nifi::minifi::api::core
