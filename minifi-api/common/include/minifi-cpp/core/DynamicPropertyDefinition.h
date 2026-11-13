// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string_view>

namespace org::apache::nifi::minifi::core {

struct DynamicPropertyDefinition {
  std::string_view name;
  std::string_view value;
  std::string_view description;
  bool supports_expression_language = false;
};

}  // namespace org::apache::nifi::minifi::core
