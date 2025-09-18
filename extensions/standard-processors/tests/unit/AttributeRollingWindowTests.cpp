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
#include <string_view>
#include "unit/Catch.h"
#include "AttributeRollingWindow.h"
#include "unit/SingleProcessorTestController.h"
#include "minifi-cpp/core/FlowFile.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {
using AttributeRollingWindow = processors::AttributeRollingWindow;

bool checkAttributes(const std::map<std::string, std::string>& expected, const std::map<std::string, std::string>& actual) {
  // expected may be incomplete, but if something is specified in expected, they also need to be in the actual
  // set of attributes
  return std::all_of(std::begin(expected), std::end(expected), [&actual](const auto& kvpair) {
    const auto& key = kvpair.first;
    const auto& value = kvpair.second;
    return actual.contains(key) && actual.at(key) == value;
  });
}

TEST_CASE("AttributeRollingWindow properly forwards properties to RollingWindow and sets attributes", "[attributerollingwindow]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<AttributeRollingWindow>("AttributeRollingWindow")};
  const auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, AttributeRollingWindow::ValueToTrack, "${value}");
  controller.plan->setProperty(proc, AttributeRollingWindow::WindowLength, "3");
  const auto trigger_with_value_and_check_attributes = [&controller](const std::string& value, const std::map<std::string, std::string>& expected_out_attributes) {
    const auto rel = [](auto name) { return core::Relationship{std::move(name), "description"}; };
    const auto out = controller.trigger({.content = "content", .attributes = {{"value", value}}});
    REQUIRE(out.at(rel("failure")).empty());
    const auto out_flow_files = out.at(rel("success"));
    REQUIRE(out_flow_files.size() == 1);
    const auto out_attrs = out_flow_files[0]->getAttributes();
    REQUIRE(checkAttributes(expected_out_attributes, out_attrs));
  };
  trigger_with_value_and_check_attributes("1", {
      // [1]
      {"value", "1"},
      {"rolling.window.count", "1.000000"},
      {"rolling.window.value", "1.000000"},
      {"rolling.window.mean", "1.000000"},
      {"rolling.window.variance", "0.000000"},
      {"rolling.window.stddev", "0.000000"},
      {"rolling.window.median", "1.000000"},
      {"rolling.window.min", "1.000000"},
      {"rolling.window.max", "1.000000"}
  });
  trigger_with_value_and_check_attributes("3", {
      // [1, 3]
      {"value", "3"},
      {"rolling.window.count", "2.000000"},
      {"rolling.window.value", "4.000000"},
      {"rolling.window.mean", "2.000000"},
      {"rolling.window.variance", "1.000000"},
      {"rolling.window.stddev", "1.000000"},
      {"rolling.window.median", "2.000000"},
      {"rolling.window.min", "1.000000"},
      {"rolling.window.max", "3.000000"}
  });
  trigger_with_value_and_check_attributes("6", {
      // [1, 3, 6]
      {"value", "6"},
      {"rolling.window.count", "3.000000"},
      {"rolling.window.value", "10.000000"},
      {"rolling.window.mean", "3.333333"},
      {"rolling.window.variance", "4.222222"},
      {"rolling.window.stddev", "2.054805"},
      {"rolling.window.median", "3.000000"},
      {"rolling.window.min", "1.000000"},
      {"rolling.window.max", "6.000000"}
  });
  trigger_with_value_and_check_attributes("9", {
      // [3, 6, 9]
      {"value", "9"},
      {"rolling.window.count", "3.000000"},
      {"rolling.window.value", "18.000000"},
      {"rolling.window.mean", "6.000000"},
      {"rolling.window.variance", "6.000000"},
      {"rolling.window.stddev", "2.449490"},
      {"rolling.window.median", "6.000000"},
      {"rolling.window.min", "3.000000"},
      {"rolling.window.max", "9.000000"}
  });
}

}  // namespace org::apache::nifi::minifi::test
