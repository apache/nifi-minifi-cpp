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
#pragma once

#include "core/ParameterProvider.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::test {

class DummyParameterProvider : public core::ParameterProvider {
 public:
  using core::ParameterProvider::ParameterProvider;

  static constexpr const char* Description = "A parameter provider that returns dummy1, dummy2, and dummy3 parameters in the dummycontext.";
  static constexpr auto Dummy1Value = core::PropertyDefinitionBuilder<>::createProperty("Dummy1 Value")
      .withDescription("Value of dummy1 parameter")
      .withDefaultValue("default1")
      .build();
  static constexpr auto Dummy2Value = core::PropertyDefinitionBuilder<>::createProperty("Dummy2 Value")
      .withDescription("Value of dummy2 parameter")
      .withDefaultValue("default2")
      .build();
  static constexpr auto Dummy3Value = core::PropertyDefinitionBuilder<>::createProperty("Dummy3 Value")
      .withDescription("Value of dummy3 parameter")
      .withDefaultValue("default3")
      .build();

  void initialize() override {
    setSupportedProperties(minifi::utils::array_cat(core::ParameterProvider::Properties, std::to_array<core::PropertyReference>({Dummy1Value, Dummy2Value, Dummy3Value})));
  }

 protected:
  std::vector<core::ParameterGroup> buildParameterGroups() override {
    core::ParameterGroup group;
    std::unordered_map<std::string, std::string> parameter_map;
    auto dummy_value = getProperty("Dummy1 Value");
    parameter_map.emplace("dummy1", dummy_value.value());
    dummy_value = getProperty("Dummy2 Value");
    parameter_map.emplace("dummy2", dummy_value.value());
    dummy_value = getProperty("Dummy3 Value");
    parameter_map.emplace("dummy3", dummy_value.value());
    group.parameters = parameter_map;
    group.name = "dummycontext";
    return {group};
  }
};

}  // namespace org::apache::nifi::minifi::test
