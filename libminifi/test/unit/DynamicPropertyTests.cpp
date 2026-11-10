// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>
#include "core/ConfigurableComponentImpl.h"
#include "utils/PropertyErrors.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::core {

class TestConfigurableComponentSupportsDynamic : public ConfigurableComponentImpl, public CoreComponentImpl {
 public:
  TestConfigurableComponentSupportsDynamic() : ConfigurableComponentImpl(), CoreComponentImpl("TestConfigurableComponentSupportsDynamic") {}

  bool supportsDynamicProperties() const override {
    return true;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  bool canEdit() override {
    return false;
  }
};

class TestConfigurableComponentNotSupportsDynamic : public ConfigurableComponentImpl, public CoreComponentImpl {
 public:
  TestConfigurableComponentNotSupportsDynamic() : ConfigurableComponentImpl(), CoreComponentImpl("TestConfigurableComponentNotSupportsDynamic") {}
  bool supportsDynamicProperties() const override {
    return false;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  bool canEdit() override {
    return false;
  }
};

TEST_CASE("Test Set Dynamic Property", "[testSetDynamicProperty]") {
  TestConfigurableComponentSupportsDynamic component;
  CHECK(component.setDynamicProperty("test", "value"));
  CHECK("value" == component.getDynamicProperty("test"));
}

TEST_CASE("Test Set Dynamic Property 2", "[testSetDynamicProperty2]") {
  TestConfigurableComponentSupportsDynamic component;
  CHECK(component.setDynamicProperty("test", "value"));
  CHECK(component.setDynamicProperty("test", "value2"));
  CHECK("value2" == component.getDynamicProperty("test"));
  CHECK(component.getDynamicProperty("test2").error() == PropertyErrorCode::PropertyNotSet);
}

TEST_CASE("Test Set Dynamic Property Fail", "[testSetDynamicPropertyFail]") {
  TestConfigurableComponentNotSupportsDynamic component;
  CHECK(component.setDynamicProperty("test", "value").error() == PropertyErrorCode::DynamicPropertiesNotSupported);
  CHECK(component.getDynamicProperty("test").error() == PropertyErrorCode::DynamicPropertiesNotSupported);
}

TEST_CASE("Test Set Dynamic Property 3", "[testSetDynamicProperty2]") {
  TestConfigurableComponentSupportsDynamic component;
  CHECK(component.setDynamicProperty("test", "value"));
  CHECK(component.setDynamicProperty("test2", "value2"));
  auto propertyKeys = component.getDynamicPropertyKeys();
  REQUIRE(2 == propertyKeys.size());
  REQUIRE("test" == propertyKeys[0]);
  REQUIRE("test2" == propertyKeys[1]);
}

}  // namespace org::apache::nifi::minifi::core
