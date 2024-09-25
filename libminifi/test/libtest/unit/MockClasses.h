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

#include <memory>
#include <string>
#include <utility>

#include "core/controller/ControllerService.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "Catch.h"

namespace minifi = org::apache::nifi::minifi;

std::atomic<bool> disabled;
std::mutex control_mutex;

class MockControllerService : public minifi::core::controller::ControllerService {
 public:
  explicit MockControllerService(std::string_view name, const minifi::utils::Identifier &uuid)
      : ControllerService(name, uuid) {
  }

  explicit MockControllerService(std::string_view name)
      : ControllerService(name) {
  }
  MockControllerService() = default;

  ~MockControllerService() override = default;

  static constexpr const char* Description = "An example service";
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {
    minifi::core::controller::ControllerServiceImpl::initialize();
    enable();
  }

  std::string doSomething() {
    return str;
  }

  virtual void enable() {
    str = "pushitrealgood";
  }

  void yield() override {
  }

  bool isRunning() const override {
    return true;
  }

  bool isWorkAvailable() override {
    return true;
  }

 protected:
  std::string str;
};

class MockProcessor : public minifi::core::Processor {
 public:
  explicit MockProcessor(std::string_view name, const minifi::utils::Identifier &uuid)
      : Processor(name, uuid) {
    setTriggerWhenEmpty(true);
  }

  explicit MockProcessor(std::string_view name)
      : Processor(name) {
    setTriggerWhenEmpty(true);
  }

  ~MockProcessor() override = default;

  static constexpr const char* Description = "An example processor";
  static constexpr auto LinkedService = minifi::core::PropertyDefinitionBuilder<>::createProperty("linkedService").withDescription("Linked service").build();
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 1>{LinkedService};
  static constexpr auto Relationships = std::array<minifi::core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr minifi::core::annotation::Input InputRequirement = minifi::core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(Properties);
  }

  void onTrigger(minifi::core::ProcessContext& context, minifi::core::ProcessSession&) override {
    std::string linked_service;
    getProperty("linkedService", linked_service);
    if (!IsNullOrEmpty(linked_service)) {
      std::shared_ptr<minifi::core::controller::ControllerService> service = context.getControllerService(linked_service);
      std::lock_guard<std::mutex> lock(control_mutex);
      if (!disabled.load()) {
        REQUIRE(context.isControllerServiceEnabled(linked_service));
        REQUIRE(nullptr != service);
        REQUIRE("pushitrealgood" == std::static_pointer_cast<MockControllerService>(service)->doSomething());
      } else {
        REQUIRE_FALSE(context.isControllerServiceEnabled(linked_service));
      }
      // verify we have access to the controller service
      // and verify that we can execute it.
    }
  }

  bool isYield() override {
    return false;
  }
};
