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
#include "PropertyDefinition.h"
#include "core/Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::core {
namespace {

class TestProcessor : public Processor {
 public:
  using Processor::Processor;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

SMART_ENUM(TestEnum,
  (A, "A"),
  (B, "B")
)

TEST_CASE("Parse enum property") {
  static constexpr auto prop = PropertyDefinitionBuilder<TestEnum::length>::createProperty("prop")
      .withAllowedValues(TestEnum::values)
      .build();
  auto proc = std::make_shared<TestProcessor>("test-proc");
  proc->setSupportedProperties(std::array<core::PropertyReference, 1>{prop});
  ProcessContext context(std::make_shared<ProcessorNode>(proc.get()), nullptr, nullptr, nullptr, nullptr, nullptr);
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
}

}  // namespace
}  // namespace org::apache::nifi::minifi::core
