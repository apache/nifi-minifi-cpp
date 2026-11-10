// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <string>

#include "minifi-api.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::core {

class ControllerServiceContext {
 public:
  explicit ControllerServiceContext(minifi_controller_service_context* impl) : impl_(impl) {}

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name) const;
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& property_reference) const {
    return getProperty(property_reference.name);
  }

 private:
  minifi_controller_service_context* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
