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

#include <string>
#include "core/ConfigurableComponent.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::core {

class TestConfigurableComponentSupportsDynamic : public ConfigurableComponentImpl {
 public:
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

class TestConfigurableComponentNotSupportsDynamic : public ConfigurableComponentImpl {
 public:
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
  component.setDynamicProperty("test", "value");
  std::string value;
  component.getDynamicProperty("test", value);
  REQUIRE(value == "value");
}

TEST_CASE("Test Set Dynamic Property 2", "[testSetDynamicProperty2]") {
  TestConfigurableComponentSupportsDynamic component;
  component.setDynamicProperty("test", "value");
  component.setDynamicProperty("test", "value2");
  std::string value;
  component.getDynamicProperty("test", value);
  REQUIRE(value == "value2");
}

TEST_CASE("Test Set Dynamic Property Fail", "[testSetDynamicPropertyFail]") {
  TestConfigurableComponentNotSupportsDynamic component;
  REQUIRE(!component.setDynamicProperty("test", "value"));
  std::string value;
  component.getDynamicProperty("test", value);
  REQUIRE(value == "");
}

TEST_CASE("Test Set Dynamic Property 3", "[testSetDynamicProperty2]") {
  TestConfigurableComponentSupportsDynamic component;
  component.setDynamicProperty("test", "value");
  component.setDynamicProperty("test2", "value2");
  std::string value;
  auto propertyKeys = component.getDynamicPropertyKeys();
  REQUIRE(2 == propertyKeys.size());
  REQUIRE("test" == propertyKeys[0]);
  REQUIRE("test2" == propertyKeys[1]);
}

}  // namespace org::apache::nifi::minifi::core
