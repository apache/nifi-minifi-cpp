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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/ConfigurableComponent.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/PropertyErrors.h"
#include "core/PropertyType.h"
#include "core/PropertyValue.h"

namespace org::apache::nifi::minifi::core {

/**
 * This Tests checks a deprecated behavior that should be removed
 * in the next major release.
 */
TEST_CASE("Some default values get coerced to typed variants") {
  auto prop = Property("prop", "d", "true");
  REQUIRE_THROWS_AS(prop.setValue("banana"), utils::internal::ConversionException);

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
  REQUIRE_THROWS_AS(property.setValue("not int"), utils::internal::ParseException);
  auto cast_check = [&]{ return static_cast<int>(property.getValue()) == 0; };  // To avoid unused-value warning
  REQUIRE_THROWS_AS(cast_check(), utils::internal::InvalidValueException);
}

TEST_CASE("Parsing int has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("55almost int"), utils::internal::ParseException);
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
  REQUIRE_THROWS_AS(property.setValue("  5000000000  "), utils::internal::ParseException);
}

TEST_CASE("Parsing bool has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  REQUIRE_THROWS_AS(property.setValue("false almost bool"), utils::internal::ParseException);
}

class TestConfigurableComponent : public ConfigurableComponentImpl {
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
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), utils::internal::RequiredPropertyMissingException);
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
  REQUIRE_THROWS_AS(component.setProperty("prop", "banana"), utils::internal::ParseException);
  std::string value;
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), utils::internal::InvalidValueException);
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
  int value = 0;
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
  bool value = false;
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), value), utils::internal::ConversionException);
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
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "banana"), utils::internal::ConversionException);
  component.setProperty(property.getName(), "98");
  int value = 0;
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
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "banana"), utils::internal::ConversionException);
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
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "20"), utils::internal::InvalidValueException);
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
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "20"), utils::internal::ParseException);
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
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), time_period_value), utils::internal::ValueException);
}

TEST_CASE("Validating listener port property") {
  static constexpr auto property_definition = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .build();
  Property property{property_definition};
  REQUIRE_NOTHROW(property.setValue("1234"));
  REQUIRE_THROWS_AS(property.setValue("banana"), utils::internal::InvalidValueException);
  REQUIRE_THROWS_AS(property.setValue("65536"), utils::internal::InvalidValueException);
  REQUIRE_THROWS_AS(property.setValue("-1"), utils::internal::InvalidValueException);
}

TEST_CASE("Validating data transfer speed property with default value") {
  using namespace std::literals::chrono_literals;
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withDefaultValue("10 KB/s")
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  DataTransferSpeedValue data_transfer_speed_value;
  REQUIRE(component.getProperty(property.getName(), data_transfer_speed_value));
  CHECK(data_transfer_speed_value.getValue() == 10_KiB);
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "20 MB/s"));
  REQUIRE(component.getProperty(property.getName(), data_transfer_speed_value));
  CHECK(data_transfer_speed_value.getValue() == 20_MiB);
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "1TB/S "));
  REQUIRE(component.getProperty(property.getName(), data_transfer_speed_value));
  CHECK(data_transfer_speed_value.getValue() == 1_TiB);
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "1KBinvalidsuffix"));
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), data_transfer_speed_value), utils::internal::ValueException);
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "1KB"));
  REQUIRE_THROWS_AS(component.getProperty(property.getName(), data_transfer_speed_value), utils::internal::ValueException);
}

TEST_CASE("Validating data transfer speed property without default value") {
  using namespace std::literals::chrono_literals;
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withPropertyType(core::StandardPropertyTypes::DATA_TRANSFER_SPEED_TYPE)
      .build();
  Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  DataTransferSpeedValue data_transfer_speed_value;
  REQUIRE_FALSE(component.getProperty(property.getName(), data_transfer_speed_value));
  REQUIRE_NOTHROW(component.setProperty(property.getName(), "1TB/S "));
  REQUIRE(component.getProperty(property.getName(), data_transfer_speed_value));
  CHECK(data_transfer_speed_value.getValue() == 1_TiB);
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "1KBinvalidsuffix"), utils::internal::ValueException);
  REQUIRE_THROWS_AS(component.setProperty(property.getName(), "1KB"), utils::internal::ValueException);
}

}  // namespace org::apache::nifi::minifi::core
