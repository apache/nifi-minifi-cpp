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
#pragma once

#include "core/ParameterProvider.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::parameter_providers {

enum class EnvironmentVariableInclusionStrategyOptions {
  include_all,
  comma_separated,
  regular_expression
};

}  // namespace org::apache::nifi::minifi::parameter_providers

namespace magic_enum::customize {
using EnvironmentVariableInclusionStrategyOptions = org::apache::nifi::minifi::parameter_providers::EnvironmentVariableInclusionStrategyOptions;

template <>
constexpr customize_t enum_name<EnvironmentVariableInclusionStrategyOptions>(EnvironmentVariableInclusionStrategyOptions value) noexcept {
  switch (value) {
    case EnvironmentVariableInclusionStrategyOptions::include_all:
      return "Include All";
    case EnvironmentVariableInclusionStrategyOptions::comma_separated:
      return "Comma-Separated";
    case EnvironmentVariableInclusionStrategyOptions::regular_expression:
      return "Regular Expression";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::parameter_providers {

class EnvironmentVariableParameterProvider final : public core::ParameterProvider {
 public:
  using ParameterProvider::ParameterProvider;

  MINIFIAPI static constexpr const char* Description = "Fetches parameters from environment variables";

  MINIFIAPI static constexpr auto EnvironmentVariableInclusionStrategy =
    core::PropertyDefinitionBuilder<magic_enum::enum_count<EnvironmentVariableInclusionStrategyOptions>()>::createProperty("Environment Variable Inclusion Strategy")
      .withDescription("Indicates how Environment Variables should be included")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(EnvironmentVariableInclusionStrategyOptions::include_all))
      .withAllowedValues(magic_enum::enum_names<EnvironmentVariableInclusionStrategyOptions>())
      .build();
  MINIFIAPI static constexpr auto IncludeEnvironmentVariables = core::PropertyDefinitionBuilder<>::createProperty("Include Environment Variables")
      .withDescription("Specifies comma separated environment variable names or regular expression (depending on the Environment Variable Inclusion Strategy) "
                       "that should be used to fetch environment variables.")
      .build();
  MINIFIAPI static constexpr auto ParameterGroupName = core::PropertyDefinitionBuilder<>::createProperty("Parameter Group Name")
      .withDescription("The name of the parameter group that will be fetched. This indicates the name of the Parameter Context that may receive the fetched parameters.")
      .isRequired(true)
      .build();

  void initialize() override {
    setSupportedProperties(minifi::utils::array_cat(core::ParameterProvider::Properties,
      std::to_array<core::PropertyReference>({EnvironmentVariableInclusionStrategy, IncludeEnvironmentVariables, ParameterGroupName})));
  }

 private:
  std::string readParameterGroupName() const;
  EnvironmentVariableInclusionStrategyOptions readEnvironmentVariableInclusionStrategy() const;
  void filterEnvironmentVariablesByCommaSeparatedList(std::unordered_map<std::string, std::string>& environment_variables) const;
  void filterEnvironmentVariablesByRegularExpression(std::unordered_map<std::string, std::string>& environment_variables) const;
  std::vector<core::ParameterGroup> buildParameterGroups() override;

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<EnvironmentVariableParameterProvider>::getLogger(uuid_)};
};

}  // namespace org::apache::nifi::minifi::parameter_providers
