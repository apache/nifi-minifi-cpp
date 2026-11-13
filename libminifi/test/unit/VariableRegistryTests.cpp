// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
  configuration->set("minifi.variable.registry.whitelist", "foo,foo_password,baz");
  configuration->set("minifi.variable.registry.blacklist", "foo");
  configuration->set("bar", "bar_val");
  configuration->set("baz", "baz_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo"));  // whitelisted but also blacklisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("bar"));  // not whitelisted
  CHECK(variable_registry.getConfigurationProperty("baz") == "baz_val");  // whitelisted
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // passwords are blacklisted
}

TEST_CASE("Explicit empty whitelist") {
  namespace minifi = org::apache::nifi::minifi;
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("minifi.variable.registry.whitelist", "");
  configuration->set("foo", "foo_val");
  configuration->set("bar", "bar_val");
  configuration->set("baz", "baz_val");
  configuration->set("foo_password", "secret password");
  const auto variable_registry = minifi::core::VariableRegistryImpl(configuration);
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo"));  // empty whitelist
  CHECK_FALSE(variable_registry.getConfigurationProperty("bar"));  // empty whitelist
  CHECK_FALSE(variable_registry.getConfigurationProperty("baz"));  // empty whitelist
  CHECK_FALSE(variable_registry.getConfigurationProperty("foo_password"));  // empty whitelist (and also passwords are blacklisted)
}
