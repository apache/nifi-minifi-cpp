// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
