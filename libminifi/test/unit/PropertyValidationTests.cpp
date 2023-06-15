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

#include "../TestBase.h"
#include "../Catch.h"
#include "core/ConfigurableComponent.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/PropertyErrors.h"
#include "core/PropertyType.h"

namespace org::apache::nifi::minifi::core {

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
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("not int"), ParseException);
  auto cast_check = [&]{ return static_cast<int>(property.getValue()) == 0; };  // To avoid unused-value warning
  REQUIRE_THROWS_AS(cast_check(), InvalidValueException);
}

TEST_CASE("Parsing int has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("55almost int"), ParseException);
}

TEST_CASE("Parsing int has spaces") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  property.setValue("  55  ");
  REQUIRE(static_cast<int>(property.getValue()) == 55);
}

TEST_CASE("Parsing int out of range") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("  5000000000  "), ParseException);
}

TEST_CASE("Parsing bool has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("false almost bool"), ParseException);
}

class TestConfigurableComponent : public ConfigurableComponent {
 public:
  bool supportsDynamicProperties() const override {
    return true;
  }

  bool supportsDynamicRelationships() const override {
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
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .isRequired(true)
      .withDefaultValue("default")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  std::string value;
  REQUIRE(component.getProperty(property.getName(), value));
  REQUIRE(value == "default");
}

TEST_CASE("Missing Required Without Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .isRequired(true)
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  std::string value;
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), RequiredPropertyMissingException);
}

TEST_CASE("Missing Optional Without Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .isRequired(false)
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  std::string value;
  REQUIRE_FALSE(component.getProperty(property.getName(), value));
}

TEST_CASE("Valid Optional Without Default") {
  // without a default the value will be stored as a string
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .isRequired(false)
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  component.setProperty(property.getName(), "some data");
  std::string value;
  REQUIRE(component.getProperty(property.getName(), value));
  REQUIRE(value == "some data");
}

TEST_CASE("Invalid With Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  REQUIRE_THROWS_AS(component.setProperty("prop", "banana"), ParseException);
  std::string value;
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), InvalidValueException);
}

TEST_CASE("Valid With Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("55")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  REQUIRE(component.setProperty("prop", "23"));
  int value;
  REQUIRE(component.getProperty(property.getName(), value));
  REQUIRE(value == 23);
}

TEST_CASE("Invalid conversion") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withDefaultValue("banana")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  bool value;
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), ConversionException);
}

TEST_CASE("Write Invalid Then Override With Valid") {
  // we always base the assignment on the default value
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("55")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "banana"), ConversionException);
  component.setProperty(property.getName(), "98");
  int value;
  REQUIRE(component.getProperty(property.getName(), value));
  REQUIRE(value == 98);
}

TEST_CASE("Property Change notification gets called even on erroneous assignment") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  int callbackCount = 0;
  component.setPropertyModifiedCallback([&](const Property &, const Property &) {
    ++callbackCount;
  });
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "banana"), ConversionException);
  REQUIRE(callbackCount == 1);
}

TEST_CASE("Correctly Typed Property With Invalid Validation") {
  static constexpr LongPropertyType between_zero_and_ten{0, 10};
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(between_zero_and_ten)
      .withDefaultValue("5")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  int callbackCount = 0;
  component.setPropertyModifiedCallback([&](const Property &, const Property &) {
    ++callbackCount;
  });
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "20"), InvalidValueException);
  REQUIRE(callbackCount == 1);
}

TEST_CASE("TimePeriodValue Property") {
  using namespace std::literals::chrono_literals;
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 minutes")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  TimePeriodValue time_period_value;
  REQUIRE(component.getProperty(property.getName(), time_period_value));
  CHECK(time_period_value.getMilliseconds() == 10min);
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "20"), ParseException);
}

TEST_CASE("TimePeriodValue Property without validator") {
  using namespace std::literals::chrono_literals;
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withDefaultValue("60 minutes")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  TimePeriodValue time_period_value;
  REQUIRE(component.getProperty(property.getName(), time_period_value));
  CHECK(time_period_value.getMilliseconds() == 1h);
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "20"));
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), time_period_value), ValueException);
}

TEST_CASE("Validating listener port property") {
  static constexpr auto property_definition = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .build();
  Property property{property_definition};
  REQUIRE_NOTHROW(property.setValue("1234"));
  REQUIRE_THROWS_AS(property.setValue("banana"), InvalidValueException);
  REQUIRE_THROWS_AS(property.setValue("65536"), InvalidValueException);
  REQUIRE_THROWS_AS(property.setValue("-1"), InvalidValueException);
}

}  // namespace org::apache::nifi::minifi::core
