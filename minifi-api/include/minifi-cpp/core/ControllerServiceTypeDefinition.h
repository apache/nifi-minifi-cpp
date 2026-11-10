// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string_view>

namespace org::apache::nifi::minifi::core {

struct ControllerServiceTypeDefinition {
  std::string_view artifact;
  std::string_view group;
  std::string_view type;
  std::string_view version;

  auto operator<=>(const ControllerServiceTypeDefinition&) const = default;
};

}  // namespace org::apache::nifi::minifi::core
