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
#define EXTENSION_LIST ""  // NOLINT(cppcoreguidelines-macro-usage)
#include <array>
#include "unit/Catch.h"
#include "core/AbstractProcessor.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::test {

struct AbstractProcessorTestCase1 : core::AbstractProcessor<AbstractProcessorTestCase1> {
  static constexpr auto SupportsDynamicProperties = true;
  static constexpr auto SupportsDynamicRelationships = true;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  static constexpr auto IsSingleThreaded = true;
  static constexpr auto Property1 = core::PropertyDefinitionBuilder<>::createProperty("Property 1")
      .withDefaultValue("foo")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  static constexpr auto Property2 = core::PropertyDefinitionBuilder<>::createProperty("Property 2")
      .withDefaultValue("bar")
      .supportsExpressionLanguage(false)
      .isRequired(false)
      .build();
  static constexpr auto Properties = std::to_array<core::PropertyReference>({Property1, Property2});
  static constexpr core::RelationshipDefinition Rel1{"rel1", "rel1 description"};
  static constexpr auto Relationships = std::array{Rel1};
  AbstractProcessorTestCase1() :core::AbstractProcessor<AbstractProcessorTestCase1>{"TestCase1"} {}
  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override {}
  void onTrigger(core::ProcessContext&, core::ProcessSession&) override {}
};

TEST_CASE("AbstractProcessor case1", "[processor][abstractprocessor][case1]") {
  AbstractProcessorTestCase1 processor;
  processor.initialize();
  REQUIRE(processor.getName() == "TestCase1");
  REQUIRE(processor.supportsDynamicProperties() == true);
  REQUIRE(processor.supportsDynamicRelationships() == true);
  REQUIRE(processor.getInputRequirement() == core::annotation::Input::INPUT_FORBIDDEN);
  REQUIRE(processor.isSingleThreaded());
  const auto properties = processor.getSupportedProperties();
  REQUIRE(properties.size() == 2);
  REQUIRE(properties.contains("Property 1"));
  REQUIRE(properties.at("Property 1").supportsExpressionLanguage());
  REQUIRE(properties.at("Property 1").getRequired());
  REQUIRE(properties.contains("Property 2"));
  REQUIRE(!properties.at("Property 2").supportsExpressionLanguage());
  REQUIRE(!properties.at("Property 2").getRequired());
  REQUIRE(processor.getSupportedRelationships().size() == 1);
  const auto relationships = processor.getSupportedRelationships();
  REQUIRE(relationships[0].getName() == "rel1");
}

struct AbstractProcessorTestCase2 : core::AbstractProcessor<AbstractProcessorTestCase2> {
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  static constexpr bool IsSingleThreaded = false;
  static constexpr auto Property1 = core::PropertyDefinitionBuilder<>::createProperty("prop 1")
      .withDefaultValue("foo")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  static constexpr auto Property2 = core::PropertyDefinitionBuilder<>::createProperty("prop 2")
      .withDefaultValue("bar")
      .supportsExpressionLanguage(false)
      .isRequired(false)
      .build();
  static constexpr auto Properties = std::to_array<core::PropertyReference>({Property1, Property2});
  static constexpr core::RelationshipDefinition Rel1{"Relationship1", "rel1 description"};
  static constexpr auto Relationships = std::array{Rel1};
  AbstractProcessorTestCase2() :core::AbstractProcessor<AbstractProcessorTestCase2>{"TestCase2"} {}
  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override {}
  void onTrigger(core::ProcessContext&, core::ProcessSession&) override {}
};

TEST_CASE("AbstractProcessor case2", "[processor][abstractprocessor][case2]") {
  AbstractProcessorTestCase2 processor;
  processor.initialize();
  REQUIRE(processor.getName() == "TestCase2");
  REQUIRE(!processor.supportsDynamicProperties());
  REQUIRE(!processor.supportsDynamicRelationships());
  REQUIRE(processor.getInputRequirement() == core::annotation::Input::INPUT_REQUIRED);
  REQUIRE(!processor.isSingleThreaded());
  const auto properties = processor.getSupportedProperties();
  REQUIRE(properties.size() == 2);
  REQUIRE(properties.contains("prop 1"));
  REQUIRE(properties.at("prop 1").supportsExpressionLanguage());
  REQUIRE(properties.at("prop 1").getRequired());
  REQUIRE(properties.contains("prop 2"));
  REQUIRE(!properties.at("prop 2").supportsExpressionLanguage());
  REQUIRE(!properties.at("prop 2").getRequired());
  REQUIRE(processor.getSupportedRelationships().size() == 1);
  const auto relationships = processor.getSupportedRelationships();
  REQUIRE(relationships[0].getName() == "Relationship1");
}

}  // namespace org::apache::nifi::minifi::test
