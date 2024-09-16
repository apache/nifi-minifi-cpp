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
#include "core/PropertyDefinition.h"
#include "core/Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/Enum.h"
#include "core/ProcessorNode.h"

namespace org::apache::nifi::minifi::core {
namespace {

class TestProcessor : public ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

enum class TestEnum {
  A,
  B
};

TEST_CASE("Parse enum property") {
  static constexpr auto prop = PropertyDefinitionBuilder<magic_enum::enum_count<TestEnum>()>::createProperty("prop")
      .withAllowedValues(magic_enum::enum_names<TestEnum>())
      .build();
  auto proc = std::make_shared<TestProcessor>("test-proc");
  proc->setSupportedProperties(std::to_array<core::PropertyReference>({prop}));
  ProcessContextImpl context(std::make_shared<ProcessorNodeImpl>(proc.get()), nullptr, nullptr, nullptr, nullptr, nullptr);
  SECTION("Valid") {
    proc->setProperty(prop, "B");
    const auto val = utils::parseEnumProperty<TestEnum>(context, prop);
    REQUIRE(val == TestEnum::B);
  }
  SECTION("Invalid") {
    proc->setProperty(prop, "C");
    REQUIRE_THROWS(utils::parseEnumProperty<TestEnum>(context, prop));
  }
  SECTION("Missing") {
    REQUIRE_THROWS(utils::parseEnumProperty<TestEnum>(context, prop));
  }
  SECTION("Optional enum property valid") {
    proc->setProperty(prop, "B");
    const auto val = utils::parseOptionalEnumProperty<TestEnum>(context, prop);
    REQUIRE(*val == TestEnum::B);
  }
  SECTION("Optional enum property invalid") {
    proc->setProperty(prop, "C");
    REQUIRE_THROWS(utils::parseOptionalEnumProperty<TestEnum>(context, prop));
  }
  SECTION("Optional enum property missing") {
    const auto val = utils::parseOptionalEnumProperty<TestEnum>(context, prop);
    REQUIRE(val == std::nullopt);
  }
}

}  // namespace
}  // namespace org::apache::nifi::minifi::core
