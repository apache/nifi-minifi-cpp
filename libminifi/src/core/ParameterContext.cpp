/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "core/ParameterContext.h"

namespace org::apache::nifi::minifi::core {

void ParameterContext::addParameter(const Parameter &parameter) {
  if (parameters_.find(parameter.name) != parameters_.end()) {
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
