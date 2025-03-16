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
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {


TEST_CASE("Converting invalid PropertyValue") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  CHECK_FALSE(property.setValue("not int").has_value());
  CHECK(property.getValue() == "0");
}

TEST_CASE("Parsing int has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  CHECK_FALSE(property.setValue("not int").has_value());
  CHECK(property.getValue() == "0");
}

TEST_CASE("Parsing int has spaces") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .build();
  Property property{property_definition};
  CHECK(property.setValue("  55  ").has_value());
  CHECK(property.getValue() == "  55  ");
  CHECK_FALSE(property.setValue("  10000000000000000000  "));
}

TEST_CASE("Parsing bool has baggage after") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  CHECK_FALSE(property.setValue("false almost bool").has_value());
  CHECK(property.getValue() == "true");
}

TEST_CASE("NON_BLANK_VALIDATOR test") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::NON_BLANK_VALIDATOR)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  CHECK_FALSE(property.setValue(""));
  CHECK(property.setValue("foo"));
}

TEST_CASE("UNSIGNED_INTEGER_VALIDATOR test") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  CHECK_FALSE(property.setValue(" -12"));
  CHECK_FALSE(property.setValue(" 1000000000000000000000"));
  CHECK(property.setValue("  231  "));
}

TEST_CASE("DATA_SIZE_VALIDATOR test") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::DATA_SIZE_VALIDATOR)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  CHECK(property.setValue("100 MaciBytes"));
  CHECK(property.setValue("100 KB "));
  CHECK(property.setValue("  100 KB  "));
  CHECK_FALSE(property.setValue("GB"));
  CHECK_FALSE(property.setValue("foo"));
}

TEST_CASE("PORT_VALIDATOR test") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::PORT_VALIDATOR)
      .withDefaultValue("true")
      .build();
  Property property{property_definition};
  CHECK(property.setValue("0"));
  CHECK(property.setValue(" 65535"));
  CHECK_FALSE(property.setValue("65536"));
  CHECK_FALSE(property.setValue("  -2  "));
  CHECK_FALSE(property.setValue("foo"));
}

class TestConfigurableComponent : public ConfigurableComponentImpl, public CoreComponentImpl {
 public:
  TestConfigurableComponent() : ConfigurableComponentImpl(), CoreComponentImpl("TestConfigurableComponent") {}

  bool supportsDynamicProperties() const override {
    return true;
  }

  bool supportsDynamicRelationships() const override {
    return true;
  }

  bool canEdit() override {
    return true;
  }
};

TEST_CASE("Missing Required With Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop").isRequired(true).withDefaultValue("default").build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.getProperty(property.getName()) == "default");
}

TEST_CASE("Missing Required Without Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop").isRequired(true).build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.getProperty(property.getName()).error() == core::PropertyErrorCode::PropertyNotSet);
}

TEST_CASE("Missing Optional Without Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .isRequired(false)
      .build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.getProperty(property.getName()).error() == core::PropertyErrorCode::PropertyNotSet);
}

TEST_CASE("Valid Optional Without Default") {
  // without a default the value will be stored as a string
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop").isRequired(false).build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.setProperty(property.getName(), "some data"));
  std::string value;
  CHECK(component.getProperty(property.getName()) == "some data");
}

TEST_CASE("Invalid With Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.setProperty(property.getName(), "banana").error() == core::PropertyErrorCode::ValidationFailed);
  CHECK(component.getProperty(property.getName()) == "true");
}

TEST_CASE("Valid With Default") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::INTEGER_VALIDATOR)
      .withDefaultValue("55")
      .build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.setProperty("prop", "23"));
  CHECK(component.getProperty(property.getName()) == "23");
}

TEST_CASE("Write Invalid Then Override With Valid") {
  static constexpr auto property_definition = PropertyDefinitionBuilder<>::createProperty("prop")
      .withValidator(core::StandardPropertyTypes::INTEGER_VALIDATOR)
      .withDefaultValue("55")
      .build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.setProperty(property.getName(), "banana").error() == core::PropertyErrorCode::ValidationFailed);
  CHECK(component.setProperty(property.getName(), "98"));
  CHECK(component.getProperty(property.getName()) == "98");
}

TEST_CASE("TimePeriodValue Property") {
  using namespace std::literals::chrono_literals;
  static constexpr auto property_definition =
      PropertyDefinitionBuilder<>::createProperty("prop").withValidator(core::StandardPropertyTypes::TIME_PERIOD_VALIDATOR).withDefaultValue("10 minutes").build();
  const Property property{property_definition};
  TestConfigurableComponent component;
  component.setSupportedProperties(std::array<PropertyReference, 1>{property_definition});
  CHECK(component.getProperty(property.getName()) == "10 minutes");
  CHECK(component.setProperty(property.getName(), "20").error() == core::PropertyErrorCode::ValidationFailed);
}

}  // namespace org::apache::nifi::minifi::core
