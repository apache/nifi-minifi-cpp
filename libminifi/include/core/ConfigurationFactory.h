// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "FlowConfiguration.h"

namespace org::apache::nifi::minifi::core {

template<typename T>
T* instantiate(ConfigurationContext ctx) {
  return new T(std::move(ctx));
}

/**
 * Configuration factory is used to create a new FlowConfiguration
 * object.
 */
std::unique_ptr<core::FlowConfiguration> createFlowConfiguration(const ConfigurationContext& ctx, const std::optional<std::string>& configuration_class_name, bool fail_safe = false);

}  // namespace org::apache::nifi::minifi::core
