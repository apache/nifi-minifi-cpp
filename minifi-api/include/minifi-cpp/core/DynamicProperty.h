// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include "minifi-cpp/core/DynamicPropertyDefinition.h"

namespace org::apache::nifi::minifi::core {

struct DynamicProperty {
  std::string name;
  std::string value;
  std::string description;
  bool supports_expression_language = false;

  DynamicProperty(const DynamicPropertyDefinition& definition)  // NOLINT(runtime/explicit)
    : name(definition.name),
      value(definition.value),
      description(definition.description),
      supports_expression_language(definition.supports_expression_language) {}
};

}  // namespace org::apache::nifi::minifi::core
