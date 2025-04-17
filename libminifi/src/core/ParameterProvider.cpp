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
#include "core/ParameterProvider.h"

namespace org::apache::nifi::minifi::core {

bool ParameterProvider::reloadValuesOnRestart() const {
  if (auto reload_values_on_restart_str = getProperty(ReloadValuesOnRestart.name)) {
    if (auto reload_values_on_restart = parsing::parseBool(*reload_values_on_restart_str)) {
      return reload_values_on_restart.value();
    }
  }
  throw ParameterException("Reload Values On Restart property is required");
}

SensitiveParameterConfig ParameterProvider::readSensitiveParameterConfig() const {
  SensitiveParameterConfig config;

  auto sensitive_parameter_scope_str = getProperty(SensitiveParameterScope.name);
  if (!sensitive_parameter_scope_str) {
    throw ParameterException("Sensitive Parameter Scope is required");
  }
  auto sensitive_parameter_scope = magic_enum::enum_cast<SensitiveParameterScopeOptions>(*sensitive_parameter_scope_str);
  if (!sensitive_parameter_scope) {
    throw ParameterException("Sensitive Parameter Scope has invalid value: '" + *sensitive_parameter_scope_str + "'");
  }
  config.sensitive_parameter_scope = sensitive_parameter_scope.value();

  if (config.sensitive_parameter_scope == SensitiveParameterScopeOptions::selected) {
    if (auto sensitive_parameter_list = getProperty(SensitiveParameterList.name)) {
      for (const auto& sensitive_parameter : minifi::utils::string::splitAndTrimRemovingEmpty(*sensitive_parameter_list, ",")) {
        config.sensitive_parameters.insert(sensitive_parameter);
      }
    }
    if (config.sensitive_parameters.empty()) {
      throw ParameterException("Sensitive Parameter Scope is set to 'selected' but Sensitive Parameter List is empty");
    }
  }

  return config;
}

std::vector<gsl::not_null<std::unique_ptr<ParameterContext>>> ParameterProvider::createParameterContexts() {
  auto config = readSensitiveParameterConfig();

  auto parameter_groups = buildParameterGroups();
  std::vector<gsl::not_null<std::unique_ptr<ParameterContext>>> result;
  for (const auto& parameter_group : parameter_groups) {
    auto parameter_context = std::make_unique<ParameterContext>(parameter_group.name);
    parameter_context->setParameterProvider(getUUIDStr());
    for (const auto& [name, value] : parameter_group.parameters) {
      parameter_context->addParameter(Parameter{
        .name = name,
        .description = "",
        .sensitive = config.isSensitive(name),
        .provided = true,
        .value = value
      });
    }
    result.push_back(gsl::make_not_null(std::move(parameter_context)));
  }

  return result;
}

}  // namespace org::apache::nifi::minifi::core
