// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <map>
#include "minifi-cpp/properties/Configure.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core {

/**
 * Defines a base variable registry for minifi agents.
 */
class VariableRegistry {
 public:
  virtual ~VariableRegistry() = default;
  [[nodiscard]] virtual std::optional<std::string> getConfigurationProperty(std::string_view key) const = 0;
};

}  // namespace org::apache::nifi::minifi::core
