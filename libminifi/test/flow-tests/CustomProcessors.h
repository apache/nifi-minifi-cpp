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

#pragma once

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/Processor.h"
#include "TestBase.h"
#include "Catch.h"
#include "processors/GenerateFlowFile.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

static constexpr auto Apple = core::RelationshipDefinition{"apple", ""};
static constexpr auto Banana = core::RelationshipDefinition{"banana", ""};
// The probability that this processor routes to Apple
static constexpr auto AppleProbability = core::PropertyDefinitionBuilder<>::createProperty("AppleProbability")
    .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
    .withDefaultValue("100")
    .isRequired(true)
    .build();
// The probability that this processor routes to Banana
static constexpr auto BananaProbability = core::PropertyDefinitionBuilder<>::createProperty("BananaProbability")
    .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
    .withDefaultValue("0")
    .isRequired(true)
    .build();

class ProcessorWithStatistics {
 public:
  std::atomic<int> trigger_count{0};
  std::function<void()> onTriggerCb_;
};

class TestProcessor : public core::Processor, public ProcessorWithStatistics {
 public:
  TestProcessor(std::string name, const utils::Identifier& uuid) : Processor(std::move(name), uuid) {}
  explicit TestProcessor(const std::string& name) : Processor(name) {}

  static constexpr const char* Description = "Processor used for testing cycles";
  static constexpr auto Properties = std::array<core::PropertyReference, 2>{AppleProbability, BananaProbability};
  static constexpr auto Relationships = std::array{Apple, Banana};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(Properties);
    setSupportedRelationships(Relationships);
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) override {
    ++trigger_count;
    if (onTriggerCb_) {
      onTriggerCb_();
    }
    auto flowFile = session->get();
    if (!flowFile) return;
    std::random_device rd{};
    std::uniform_int_distribution<int> dis(0, 100);
    int rand = dis(rd);
    if (rand <= apple_probability_) {
      session->transfer(flowFile, Apple);
      return;
    }
    rand -= apple_probability_;
    if (rand <= banana_probability_) {
      session->transfer(flowFile, Banana);
      return;
    }
    throw std::runtime_error("Couldn't route file");
  }
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) override {
    int apple;
    bool appleSuccess = context->getProperty(AppleProbability, apple);
    assert(appleSuccess);
    int banana;
    bool bananaSuccess = context->getProperty(BananaProbability, banana);
    assert(bananaSuccess);
    apple_probability_ = apple;
    banana_probability_ = banana;
  }
  std::atomic<int> apple_probability_;
  std::atomic<int> banana_probability_;
};

class TestFlowFileGenerator : public processors::GenerateFlowFile, public ProcessorWithStatistics {
 public:
  TestFlowFileGenerator(std::string name, const utils::Identifier& uuid) : GenerateFlowFile(std::move(name), uuid) {}
  explicit TestFlowFileGenerator(const std::string& name) : GenerateFlowFile(name) {}

  static constexpr const char* Description = "Processor generating files and notifying us";

  using processors::GenerateFlowFile::onTrigger;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override {
    ++trigger_count;
    if (onTriggerCb_) {
      onTriggerCb_();
    }
    GenerateFlowFile::onTrigger(context.get(), session.get());
  }
};

REGISTER_RESOURCE(TestProcessor, Processor);
REGISTER_RESOURCE(TestFlowFileGenerator, Processor);

}  // namespace org::apache::nifi::minifi::processors
