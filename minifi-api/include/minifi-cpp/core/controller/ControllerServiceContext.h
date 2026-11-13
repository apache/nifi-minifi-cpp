// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <vector>
#include <system_error>
#include <string_view>
#include "utils/expected.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceContext {
 public:
  virtual ~ControllerServiceContext() = default;

  [[nodiscard]] virtual std::expected<std::string, std::error_code> getProperty(std::string_view name) const = 0;
  [[nodiscard]] virtual std::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
