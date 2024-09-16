/**
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
#include "parameter-providers/EnvironmentVariableParameterProvider.h"
#include "core/Resource.h"
#include "utils/Environment.h"
#include "Exception.h"
#include "range/v3/view/filter.hpp"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::parameter_providers {

std::string EnvironmentVariableParameterProvider::readParameterGroupName() const {
  auto parameter_group_name = getProperty(ParameterGroupName.name);
  if (!parameter_group_name || parameter_group_name.value().empty()) {
    throw core::ParameterException("Parameter Group Name is required");
  }
  logger_->log_debug("Building Parameter Context with name: {}", parameter_group_name.value());
  return parameter_group_name.value();
}

EnvironmentVariableInclusionStrategyOptions EnvironmentVariableParameterProvider::readEnvironmentVariableInclusionStrategy() const {
  auto env_variable_inclusion_strategy_str = getProperty(EnvironmentVariableInclusionStrategy.name);
  if (!env_variable_inclusion_strategy_str) {
    throw core::ParameterException("Environment Variable Inclusion Strategy is required");
  }
  auto env_variable_inclusion_strategy = magic_enum::enum_cast<EnvironmentVariableInclusionStrategyOptions>(*env_variable_inclusion_strategy_str);
  if (!env_variable_inclusion_strategy) {
    throw core::ParameterException("Environment Variable Inclusion Strategy has invalid value: '" + *env_variable_inclusion_strategy_str + "'");
  }
  logger_->log_debug("Environment Variable Inclusion Strategy: {}", *env_variable_inclusion_strategy_str);
  return env_variable_inclusion_strategy.value();
}

void EnvironmentVariableParameterProvider::filterEnvironmentVariablesByCommaSeparatedList(std::unordered_map<std::string, std::string>& environment_variables) const {
  std::unordered_set<std::string> included_environment_variables;
  if (auto incuded_environment_variables_str = getProperty(IncludeEnvironmentVariables.name)) {
    for (const auto& included_environment_variable : minifi::utils::string::splitAndTrimRemovingEmpty(*incuded_environment_variables_str, ",")) {
      included_environment_variables.insert(included_environment_variable);
    }
    logger_->log_debug("Filtering environment variables by comma separated list: {}", *incuded_environment_variables_str);
  }
  if (!included_environment_variables.empty()) {
    environment_variables = environment_variables
      | ranges::views::filter([&included_environment_variables](const auto& kvp) { return included_environment_variables.contains(kvp.first); })
      | ranges::to<std::unordered_map<std::string, std::string>>();
  } else {
    throw core::ParameterException("Environment Variable Inclusion Strategy is set to Comma-Separated, but no value is defined in Include Environment Variables property");
  }
}

void EnvironmentVariableParameterProvider::filterEnvironmentVariablesByRegularExpression(std::unordered_map<std::string, std::string>& environment_variables) const {
  auto env_variable_regex_str = getProperty(IncludeEnvironmentVariables.name);
  if (env_variable_regex_str && !env_variable_regex_str->empty()) {
    logger_->log_debug("Filtering environment variables using regular expression: {}", *env_variable_regex_str);
    utils::Regex regex(*env_variable_regex_str);
    environment_variables = environment_variables
      | ranges::views::filter([&regex](const auto& kvp) { return minifi::utils::regexMatch(kvp.first, regex); })
      | ranges::to<std::unordered_map<std::string, std::string>>();
  } else {
    throw core::ParameterException("Environment Variable Inclusion Strategy is set to Regular Expression, but no regex is defined in Include Environment Variables property");
  }
}

std::vector<core::ParameterGroup> EnvironmentVariableParameterProvider::buildParameterGroups() {
  core::ParameterGroup group;
  group.name = readParameterGroupName();
  group.parameters = utils::Environment::getEnvironmentVariables();

  auto env_variable_inclusion_strategy = readEnvironmentVariableInclusionStrategy();
  if (env_variable_inclusion_strategy == EnvironmentVariableInclusionStrategyOptions::comma_separated) {
    filterEnvironmentVariablesByCommaSeparatedList(group.parameters);
  } else if (env_variable_inclusion_strategy == EnvironmentVariableInclusionStrategyOptions::regular_expression) {
    filterEnvironmentVariablesByRegularExpression(group.parameters);
  }
  return {group};
}

REGISTER_RESOURCE(EnvironmentVariableParameterProvider, ParameterProvider);

}  // namespace org::apache::nifi::minifi::parameter_providers
