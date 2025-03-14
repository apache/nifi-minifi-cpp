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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "parameter-providers/EnvironmentVariableParameterProvider.h"
#include "utils/Environment.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Parameter Group Name is required", "[parameterProviders]") {
  utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "");
  REQUIRE_THROWS_WITH(provider.createParameterContexts(), "Parameter Operation: Parameter Group Name is required");
}

TEST_CASE("Test EnvironmentVariableParameterProvider with default options", "[parameterProviders]") {
  utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "environment-variable-parameter-context");
  auto contexts = provider.createParameterContexts();
  REQUIRE(contexts.size() == 1);
  REQUIRE(contexts[0]->getName() == "environment-variable-parameter-context");
  auto enviroment_parameters = contexts[0]->getParameters();
  REQUIRE(!enviroment_parameters.empty());
  REQUIRE(enviroment_parameters.contains("MINIFI_DATA"));
  REQUIRE(enviroment_parameters["MINIFI_DATA"].value == "minifi_data_value");
}

TEST_CASE("Create parameter context with selected environment variables", "[parameterProviders]") {
  utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  utils::Environment::setEnvironmentVariable("MINIFI_NEW_DATA", "minifi_new_data_value");
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "environment-variable-parameter-context");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::EnvironmentVariableInclusionStrategy.name, "Comma-Separated");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::IncludeEnvironmentVariables.name, "MINIFI_DATA,MINIFI_NEW_DATA");
  auto contexts = provider.createParameterContexts();
  REQUIRE(contexts.size() == 1);
  REQUIRE(contexts[0]->getName() == "environment-variable-parameter-context");
  auto enviroment_parameters = contexts[0]->getParameters();
  REQUIRE(enviroment_parameters.size() == 2);
  REQUIRE(enviroment_parameters.contains("MINIFI_DATA"));
  REQUIRE(enviroment_parameters["MINIFI_DATA"].value == "minifi_data_value");
  REQUIRE(enviroment_parameters.contains("MINIFI_NEW_DATA"));
  REQUIRE(enviroment_parameters["MINIFI_NEW_DATA"].value == "minifi_new_data_value");
}

TEST_CASE("Environment variable list must be defined if Comma-Separated inclusion strategy is set", "[parameterProviders]") {
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "environment-variable-parameter-context");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::EnvironmentVariableInclusionStrategy.name, "Comma-Separated");
  REQUIRE_THROWS_WITH(provider.createParameterContexts(), "Parameter Operation: Environment Variable Inclusion Strategy is set to Comma-Separated, "
    "but no value is defined in Include Environment Variables property");
}

TEST_CASE("Create parameter context with regex matching environment variables", "[parameterProviders]") {
  utils::Environment::setEnvironmentVariable("MINIFI_DATA_1", "minifi_data_value");
  utils::Environment::setEnvironmentVariable("MINIFI_NEW_DATA", "minifi_new_data_value");
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "environment-variable-parameter-context");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::EnvironmentVariableInclusionStrategy.name, "Regular Expression");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::IncludeEnvironmentVariables.name, "MINIFI_DATA_.*");
  auto contexts = provider.createParameterContexts();
  REQUIRE(contexts.size() == 1);
  REQUIRE(contexts[0]->getName() == "environment-variable-parameter-context");
  auto enviroment_parameters = contexts[0]->getParameters();
  REQUIRE(enviroment_parameters.size() == 1);
  REQUIRE(enviroment_parameters.contains("MINIFI_DATA_1"));
  REQUIRE(enviroment_parameters["MINIFI_DATA_1"].value == "minifi_data_value");
}

TEST_CASE("Regex must be defined if regex inclusion strategy is set", "[parameterProviders]") {
  parameter_providers::EnvironmentVariableParameterProvider provider("EnvironmentVariableParameterProvider");
  provider.initialize();
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::ParameterGroupName.name, "environment-variable-parameter-context");
  provider.setProperty(parameter_providers::EnvironmentVariableParameterProvider::EnvironmentVariableInclusionStrategy.name, "Regular Expression");
  REQUIRE_THROWS_WITH(provider.createParameterContexts(), "Parameter Operation: Environment Variable Inclusion Strategy is set to Regular Expression, "
    "but no regex is defined in Include Environment Variables property");
}

}  // namespace org::apache::nifi::minifi::test
