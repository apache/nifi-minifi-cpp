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

#include <vector>

#include "core/Resource.h"
#include "controllers/AttributeProviderService.h"

class TestAttributeProviderService : public org::apache::nifi::minifi::controllers::AttributeProviderService {
 public:
  using AttributeProviderService::AttributeProviderService;

  static constexpr const char* Description = "An attribute provider service which provides a constant set of records.";
  static auto properties() { return std::array<core::Property, 0>{}; }
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {};
  void onEnable() override {};
  std::optional<std::vector<AttributeMap>> getAttributes() override {
    return std::vector<AttributeMap>{AttributeMap{{"color", "red"}, {"fruit", "apple"}, {"uid", "001"}, {"animal", "dog"}},
                                     AttributeMap{{"color", "yellow"}, {"fruit", "banana"}, {"uid", "004"}, {"animal", "dolphin"}}};
  }
  std::string_view name() const override { return "test"; }
};

REGISTER_RESOURCE(TestAttributeProviderService, ControllerService);
