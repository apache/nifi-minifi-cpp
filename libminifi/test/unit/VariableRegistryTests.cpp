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

#include "properties/Configure.h"
#include "core/VariableRegistry.h"
#include "unit/Catch.h"

TEST_CASE("VariableRegistry default test") {
  namespace minifi = org::apache::nifi::minifi;
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("foo", "foo_val");
  configuration->set("bar", "bar_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK(variable_registry.getConfigurationProperty("foo") == "foo_val");
  CHECK(variable_registry.getConfigurationProperty("bar") == "bar_val");
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // passwords are blacklisted
}

TEST_CASE("VariableRegistry whitelist test") {
  namespace minifi = org::apache::nifi::minifi;
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("foo", "foo_val");
  configuration->set("minifi.variable.registry.whitelist", "foo,foo_password");
  configuration->set("bar", "bar_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK(variable_registry.getConfigurationProperty("foo") == "foo_val");
  CHECK_FALSE(variable_registry.getConfigurationProperty("bar"));  // not whitelisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // passwords are blacklisted
}

TEST_CASE("VariableRegistry blacklist test") {
  namespace minifi = org::apache::nifi::minifi;
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("foo", "foo_val");
  configuration->set("minifi.variable.registry.blacklist", "foo,foo_password");
  configuration->set("bar", "bar_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK(variable_registry.getConfigurationProperty("bar") == "bar_val");
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo"));  // blacklisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // passwords are blacklisted
}

TEST_CASE("VariableRegistry whitelist and blacklist test") {
  namespace minifi = org::apache::nifi::minifi;
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("foo", "foo_val");
  configuration->set("minifi.variable.registry.whitelist", "foo,foo_password");
  configuration->set("minifi.variable.registry.blacklist", "foo");
  configuration->set("bar", "bar_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo"));  // whitelisted but also blacklisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("bar"));  // not whitelisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // passwords are blacklisted
}
