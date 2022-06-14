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

#include <memory>
#include <string>

#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "ProcessContextExpr.h"
#include "Processor.h"
#include "PropertyBuilder.h"
#include "TestBase.h"
#include "Catch.h"

namespace org::apache::nifi::minifi {

class DummyProcessor : public core::Processor {
 public:
  using core::Processor::Processor;

  static constexpr const char* Description = "A processor that does nothing.";
  static const core::Property SimpleProperty;
  static const core::Property ExpressionLanguageProperty;
  static auto properties() { return std::array{SimpleProperty, ExpressionLanguageProperty}; }
  static auto relationships() { return std::array<core::Relationship, 0>{}; }
  static constexpr bool SupportsDynamicProperties = true;
  static constexpr bool SupportsDynamicRelationships = true;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override { setSupportedProperties(properties()); }
};

const core::Property DummyProcessor::SimpleProperty{
    core::PropertyBuilder::createProperty("Simple Property")
        ->withDescription("Just a simple string property")
        ->build()};

const core::Property DummyProcessor::ExpressionLanguageProperty{
    core::PropertyBuilder::createProperty("Expression Language Property")
        ->withDescription("A property which supports expression language")
        ->supportsExpressionLanguage(true)
        ->build()};

REGISTER_RESOURCE(DummyProcessor, Processor);

}  // namespace org::apache::nifi::minifi

TEST_CASE("ProcessContextExpr can update existing processor properties", "[setProperty][getProperty]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  std::shared_ptr<minifi::core::Processor> dummy_processor = test_plan->addProcessor("DummyProcessor", "dummy_processor");
  std::shared_ptr<minifi::core::ProcessContext> context = [test_plan] { test_plan->runNextProcessor(); return test_plan->getCurrentContext(); }();
  REQUIRE(dynamic_pointer_cast<minifi::core::ProcessContextExpr>(context) != nullptr);

  SECTION("Set and get simple property") {
    SECTION("Using a Property reference parameter") {
      context->setProperty(minifi::DummyProcessor::SimpleProperty, "foo");
      CHECK(context->getProperty(minifi::DummyProcessor::SimpleProperty, nullptr) == "foo");

      context->setProperty(minifi::DummyProcessor::SimpleProperty, "bar");
      CHECK(context->getProperty(minifi::DummyProcessor::SimpleProperty, nullptr) == "bar");
    }

    SECTION("Using a string parameter") {
      context->setProperty(minifi::DummyProcessor::SimpleProperty.getName(), "foo");
      CHECK(context->getProperty(minifi::DummyProcessor::SimpleProperty, nullptr) == "foo");

      context->setProperty(minifi::DummyProcessor::SimpleProperty.getName(), "bar");
      CHECK(context->getProperty(minifi::DummyProcessor::SimpleProperty, nullptr) == "bar");
    }
  }

  SECTION("Set and get expression language property") {
    SECTION("Using a Property reference parameter") {
      context->setProperty(minifi::DummyProcessor::ExpressionLanguageProperty, "foo");
      CHECK(context->getProperty(minifi::DummyProcessor::ExpressionLanguageProperty, nullptr) == "foo");

      context->setProperty(minifi::DummyProcessor::ExpressionLanguageProperty, "bar");
      CHECK(context->getProperty(minifi::DummyProcessor::ExpressionLanguageProperty, nullptr) == "bar");
    }

    SECTION("Using a string parameter") {
      context->setProperty(minifi::DummyProcessor::ExpressionLanguageProperty.getName(), "foo");
      CHECK(context->getProperty(minifi::DummyProcessor::ExpressionLanguageProperty, nullptr) == "foo");

      context->setProperty(minifi::DummyProcessor::ExpressionLanguageProperty.getName(), "bar");
      CHECK(context->getProperty(minifi::DummyProcessor::ExpressionLanguageProperty, nullptr) == "bar");
    }
  }

  SECTION("Set and get simple dynamic property") {
    const auto simple_property{
        minifi::core::PropertyBuilder::createProperty("Simple Dynamic Property")
        ->withDescription("A simple dynamic string property")
        ->build()};
    std::string property_value;

    context->setDynamicProperty(simple_property.getName(), "foo");
    CHECK(context->getDynamicProperty(simple_property, property_value, nullptr));
    CHECK(property_value == "foo");

    context->setDynamicProperty(simple_property.getName(), "bar");
    CHECK(context->getDynamicProperty(simple_property, property_value, nullptr));
    CHECK(property_value == "bar");
  }

  SECTION("Set and get expression language dynamic property") {
    const auto expression_language_property{
        minifi::core::PropertyBuilder::createProperty("Expression Language Dynamic Property")
        ->withDescription("A dynamic property which supports expression language")
        ->supportsExpressionLanguage(true)
        ->build()};
    std::string property_value;

    context->setDynamicProperty(expression_language_property.getName(), "foo");
    CHECK(context->getDynamicProperty(expression_language_property, property_value, nullptr));
    CHECK(property_value == "foo");

    context->setDynamicProperty(expression_language_property.getName(), "bar");
    CHECK(context->getDynamicProperty(expression_language_property, property_value, nullptr));
    CHECK(property_value == "bar");
  }
}
