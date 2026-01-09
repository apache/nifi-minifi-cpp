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

#include "core/ProcessContextImpl.h"
#include "core/Resource.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi {

class DummyProcessContextExprProcessor : public core::ProcessorImpl {
 public:
  using core::ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "A processor that does nothing.";
  static constexpr auto SimpleProperty = core::PropertyDefinitionBuilder<>::createProperty("Simple Property")
      .withDescription("Just a simple string property")
      .build();
  static constexpr auto ExpressionLanguageProperty = core::PropertyDefinitionBuilder<>::createProperty("Expression Language Property")
      .withDescription("A property which supports expression language")
      .supportsExpressionLanguage(true)
      .build();
  static constexpr auto Properties = std::to_array<core::PropertyReference>({SimpleProperty, ExpressionLanguageProperty});
  static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = true;
  static constexpr bool SupportsDynamicRelationships = true;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override { setSupportedProperties(Properties); }
};

REGISTER_RESOURCE(DummyProcessContextExprProcessor, Processor);

}  // namespace org::apache::nifi::minifi

TEST_CASE("ProcessContextExpr can update existing processor properties", "[setProperty][getProperty]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  [[maybe_unused]] minifi::core::Processor* dummy_processor = test_plan->addProcessor("DummyProcessContextExprProcessor", "dummy_processor");
  std::shared_ptr<minifi::core::ProcessContext> context = [test_plan] { test_plan->runNextProcessor(); return test_plan->getCurrentContext(); }();
  REQUIRE(dynamic_pointer_cast<minifi::core::ProcessContextImpl>(context) != nullptr);

  SECTION("Set and get simple property") {
    SECTION("Using a Property reference parameter") {
      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "foo"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "foo");

      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "bar"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "bar");
    }

    SECTION("Using a string parameter") {
      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "foo"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "foo");

      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "bar"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "bar");
    }
  }

  SECTION("Set and get expression language property") {
    SECTION("Using a Property reference parameter") {
      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "foo"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "foo");

      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "bar"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "bar");
    }

    SECTION("Using a string parameter") {
      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "foo"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "foo");

      REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "bar"));
      CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "bar");
    }
  }
}

TEST_CASE("Expression language via variable registry") {
  const std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set("foo", "foo_val");
  configuration->set("bar", "bar_val");
  configuration->set("minifi.variable.registry.blacklist", "foo");

  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan(configuration);

  [[maybe_unused]] minifi::core::Processor* dummy_processor = test_plan->addProcessor("DummyProcessContextExprProcessor", "dummy_processor");
  std::shared_ptr<minifi::core::ProcessContext> context = [test_plan] { test_plan->runNextProcessor(); return test_plan->getCurrentContext(); }();
  REQUIRE(dynamic_pointer_cast<minifi::core::ProcessContextImpl>(context) != nullptr);

  {
    REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "${bar}"));
    CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "bar_val");
  }
  {
    REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, "${foo}"));
    CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::ExpressionLanguageProperty, nullptr) == "");
  }
  {
    REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "${bar}"));
    CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "${bar}");
  }
  {
    REQUIRE(context->setProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, "${foo}"));
    CHECK(context->getProperty(minifi::DummyProcessContextExprProcessor::SimpleProperty, nullptr) == "${foo}");
  }
}

TEST_CASE("ProcessContextExpr can use expression language in dynamic properties", "[getDynamicProperty][getDynamicProperties]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  std::ignore = test_plan->addProcessor("DummyProcessor", "dummy_processor");
  test_plan->runNextProcessor();
  const auto context = test_plan->getCurrentContext();
  REQUIRE(dynamic_pointer_cast<core::ProcessContextImpl>(context) != nullptr);

  core::FlowFileImpl flow_file;
  flow_file.setAttribute("attr_a", "myAttributeValue");
  REQUIRE(context->setDynamicProperty("MyDynamicProperty", "${attr_a}"));

  SECTION("Use getDynamicProperties") {
    for (size_t i = 0; i < 2; ++i) {
      auto properties = context->getDynamicProperties(&flow_file);
      REQUIRE(properties.size() == 1);
      CHECK(properties["MyDynamicProperty"] == "myAttributeValue");
    }
  }

  SECTION("Use getDynamicProperty") {
    for (size_t i = 0; i < 2; ++i) {
      auto property = context->getDynamicProperty("MyDynamicProperty", &flow_file);
      REQUIRE(property);
      CHECK(*property == "myAttributeValue");
    }
  }
}
