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

#include "TestBase.h"
#include "Catch.h"
#include "TestUtils.h"
#include "Utils.h"
#include "processors/JoltTransformJSON.h"
#include "SingleProcessorTestController.h"

namespace org::apache::nifi::minifi::test {


TEST_CASE("Shiftr successful case") {
  auto proc = std::make_shared<minifi::processors::JoltTransformJSON>("JoltProc");
  SingleProcessorTestController controller{proc};
  proc->setProperty(processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::SHIFT));
  proc->setProperty(processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": "a_out",
      "b": {
        "@": "b.self",
        "$": "b.key",
        "c": "automatic array",
        "d": "automatic array",
        "&": "b_b",
        "f": {
          "&(1,0)": "b_f_b"
        }
      }
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": 1,
      "b": {"c": 2, "d": "test", "b": [3, 4], "f": {"b": 5}}
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  utils::verifyJSON(controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0)), R"json(
    {
      "a_out": 1,
      "b": {
        "self": {"c": 2, "d": "test", "b": [3, 4], "f": {"b": 5}},
        "key": "b"
      },
      "automatic array": [2, "test"],
      "b_b": [3, 4],
      "b_f_b": 5
    }
  )json", true);
}

TEST_CASE("Shiftr multiple destination") {
  auto proc = std::make_shared<minifi::processors::JoltTransformJSON>("JoltProc");
  SingleProcessorTestController controller{proc};
  proc->setProperty(processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::SHIFT));
  proc->setProperty(processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": ["out1", "out2.inner"]
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": 1
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  utils::verifyJSON(controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0)), R"json(
    {
      "out1": 1,
      "out2": {"inner": 1}
    }
  )json", true);
}

TEST_CASE("Shiftr destination is a string or array of strings") {
  /// sanity check
  REQUIRE(minifi::processors::JoltTransformJSON::Spec::parse(R"json({"a": ["out", "out2"], "b": "out3"})json"));
  REQUIRE_FALSE(minifi::processors::JoltTransformJSON::Spec::parse(R"json({"a": 3})json"));
  REQUIRE_FALSE(minifi::processors::JoltTransformJSON::Spec::parse(R"json({"a": ["out", 1]})json"));
  REQUIRE_FALSE(minifi::processors::JoltTransformJSON::Spec::parse(R"json({"a": ["out", {"@": "invalid"}]})json"));
}

TEST_CASE("Shiftr template is correctly parsed") {
  using Template = minifi::processors::JoltTransformJSON::Spec::Template;
  REQUIRE(Template::parse("a&0b", "").value() == Template({"a", "b"}, {{0, 0}}));
  REQUIRE(Template::parse("a&12&(4,5)b&c", "").value() == Template({"a", "", "b", "c"}, {{12, 0}, {4, 5}, {0, 0}}));
}

TEST_CASE("Shiftr invalid reference") {
  /// sanity check
  REQUIRE(minifi::processors::JoltTransformJSON::Spec::parse(R"json({"a*": {"b*_*c": {"&(0,0)&(0,1)&(0,2)&(1)&(1,1)": "&(0,0)"}}, "b": "out3"})json").has_value());
}

}   // namespace org::apache::nifi::minifi::test
