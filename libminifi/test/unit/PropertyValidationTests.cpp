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

#include "../TestBase.h"
#include "core/ConfigurableComponent.h"
#include "utils/PropertyErrors.h"
#include "core/PropertyValidation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

using namespace utils::internal;
/**
 * This Tests checks a deprecated behavior that should be removed
 * in the next major release.
 */
TEST_CASE("Some default values get coerced to typed variants") {
  auto prop = Property("prop", "d", "true");
  REQUIRE_THROWS_AS(prop.setValue("banana"), ConversionException);

  const std::string SPACE = " ";
  auto prop2 = Property("prop", "d", SPACE + "true");
  prop2.setValue("banana");
}

TEST_CASE("Converting invalid PropertyValue") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(0)
      ->build();
  REQUIRE_THROWS_AS(prop.setValue("not int"), ParseException);
  auto cast_check = [&]{ return static_cast<int>(prop.getValue()) == 0; };  // To avoid unused-value warning
  REQUIRE_THROWS_AS(cast_check(), InvalidValueException);
}

TEST_CASE("Parsing int has baggage after") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(0)
      ->build();
  REQUIRE_THROWS_AS(prop.setValue("55almost int"), ParseException);
}

TEST_CASE("Parsing int has spaces") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(0)
      ->build();
  prop.setValue("  55  ");
  REQUIRE(static_cast<int>(prop.getValue()) == 55);
}

TEST_CASE("Parsing int out of range") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(0)
      ->build();
  REQUIRE_THROWS_AS(prop.setValue("  5000000000  "), ParseException);
}

TEST_CASE("Parsing bool has baggage after") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<bool>(true)
      ->build();
  REQUIRE_THROWS_AS(prop.setValue("false almost bool"), ParseException);
}

class TestConfigurableComponent : public ConfigurableComponent {
 public:
  bool supportsDynamicProperties() override {
    return true;
  }

  bool canEdit() override {
    return true;
  }

  void onPropertyModified(const Property &old_property, const Property &new_property) override {
    if (onPropertyModifiedCallback) onPropertyModifiedCallback(old_property, new_property);
  }

  void onDynamicPropertyModified(const Property &old_property, const Property &new_property) override {
    if (onDynamicPropertyModifiedCallback) onDynamicPropertyModifiedCallback(old_property, new_property);
  }

  template<typename Fn>
  void setPropertyModifiedCallback(Fn &&functor) {
    onPropertyModifiedCallback = std::forward<Fn>(functor);
  }

  template<typename Fn>
  void setDynamicPropertyModifiedCallback(Fn &&functor) {
    onDynamicPropertyModifiedCallback = std::forward<Fn>(functor);
  }

 private:
  std::function<void(const Property &, const Property &)> onPropertyModifiedCallback;
  std::function<void(const Property &, const Property &)> onDynamicPropertyModifiedCallback;
};

TEST_CASE("Missing Required With Default") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->isRequired(true)
      ->withDefaultValue<std::string>("default")
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  std::string value;
  REQUIRE(component.getProperty(prop.getName(), value));
  REQUIRE(value == "default");
}

TEST_CASE("Missing Required Without Default") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->isRequired(true)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  std::string value;
  REQUIRE_THROWS_AS(component.getProperty(prop.getName(), value), RequiredPropertyMissingException);
}

TEST_CASE("Missing Optional Without Default") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->isRequired(false)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  std::string value;
  REQUIRE_FALSE(component.getProperty(prop.getName(), value));
}

TEST_CASE("Valid Optional Without Default") {
  // without a default the value will be stored as a string
  auto prop = PropertyBuilder::createProperty("prop")
      ->isRequired(false)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  component.setProperty(prop.getName(), "some data");
  std::string value;
  REQUIRE(component.getProperty(prop.getName(), value));
  REQUIRE(value == "some data");
}

TEST_CASE("Invalid With Default") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<bool>(true)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  REQUIRE_THROWS_AS(component.setProperty("prop", "banana"), ParseException);
  std::string value;
  REQUIRE_THROWS_AS(component.getProperty(prop.getName(), value), InvalidValueException);
}

TEST_CASE("Valid With Default") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(55)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  REQUIRE(component.setProperty("prop", "23"));
  int value;
  REQUIRE(component.getProperty(prop.getName(), value));
  REQUIRE(value == 23);
}

TEST_CASE("Invalid conversion") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<std::string>("banana")
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  bool value;
  REQUIRE_THROWS_AS(component.getProperty(prop.getName(), value), ConversionException);
}

TEST_CASE("Write Invalid Then Override With Valid") {
  // we always base the assignment on the default value
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int>(55)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  REQUIRE_THROWS_AS(component.setProperty(prop.getName(), "banana"), ConversionException);
  component.setProperty(prop.getName(), "98");
  int value;
  REQUIRE(component.getProperty(prop.getName(), value));
  REQUIRE(value == 98);
}

TEST_CASE("Property Change notification gets called even on erroneous assignment") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<bool>(true)
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  int callbackCount = 0;
  component.setPropertyModifiedCallback([&](const Property &, const Property &) {
    ++callbackCount;
  });
  REQUIRE_THROWS_AS(component.setProperty(prop.getName(), "banana"), ConversionException);
  REQUIRE(callbackCount == 1);
}

TEST_CASE("Correctly Typed Property With Invalid Validation") {
  auto prop = PropertyBuilder::createProperty("prop")
      ->withDefaultValue<int64_t>(5, std::make_shared<LongValidator>("myValidator", 0, 10))
      ->build();
  TestConfigurableComponent component;
  component.setSupportedProperties({prop});
  int callbackCount = 0;
  component.setPropertyModifiedCallback([&](const Property &, const Property &) {
    ++callbackCount;
  });
  REQUIRE_THROWS_AS(component.setProperty(prop.getName(), "20"), InvalidValueException);
  REQUIRE(callbackCount == 1);
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
