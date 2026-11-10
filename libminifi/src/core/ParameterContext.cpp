// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/ParameterContext.h"

namespace org::apache::nifi::minifi::core {

void ParameterContext::addParameter(const Parameter &parameter) {
  if (parameters_.contains(parameter.name)) {
    throw ParameterException("Parameter name '" + parameter.name + "' already exists, parameter names must be unique within a parameter context!");
  }
  parameters_.emplace(parameter.name, parameter);
}

std::optional<Parameter> ParameterContext::getParameter(const std::string &name) const {
  auto it = parameters_.find(name);
  if (it != parameters_.end()) {
    return it->second;
  }
  for (const auto& parameter_context : inherited_parameter_contexts_) {
    if (auto parameter = parameter_context->getParameter(name)) {
      return parameter;
    }
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::core
