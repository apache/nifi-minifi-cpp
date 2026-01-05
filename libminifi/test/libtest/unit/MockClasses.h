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

#include "core/controller/ControllerServiceBase.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "Catch.h"

namespace minifi = org::apache::nifi::minifi;

std::mutex control_mutex;
std::atomic<bool> subprocess_controller_service_found_correctly{false};
std::atomic<bool> subprocess_controller_service_not_found_correctly{false};

class MockControllerService : public minifi::core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
 public:
  using ControllerServiceBase::ControllerServiceBase;
  MockControllerService()
    : ControllerServiceBase({.uuid = utils::Identifier{}, .name = "MockControllerService", .logger = logging::LoggerFactory<MockControllerService>::getLogger()}) {}

  ~MockControllerService() override = default;

  static constexpr const char* Description = "An example service";
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;

  void initialize() override {
    minifi::core::controller::ControllerServiceBase::initialize();
    enable();
  }

  std::string doSomething() {
    return str;
  }

  virtual void enable() {
    str = "pushitrealgood";
  }

  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

 protected:
  std::string str;
};

class MockProcessor : public minifi::core::ProcessorImpl {
 public:
  explicit MockProcessor(minifi::core::ProcessorMetadata metadata)
      : ProcessorImpl(metadata) {
    setTriggerWhenEmpty(true);
  }

  ~MockProcessor() override = default;

  static constexpr const char* Description = "An example processor";
  static constexpr auto LinkedService = minifi::core::PropertyDefinitionBuilder<>::createProperty("linkedService").withDescription("Linked service").build();
  static constexpr auto InSubProcessGroup = minifi::core::PropertyDefinitionBuilder<>::createProperty("InSubProcessGroup")
    .withValidator(minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
    .withDefaultValue("false")
    .withDescription("Is in sub process group")
    .build();
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 2>{LinkedService, InSubProcessGroup};
  static constexpr auto Success = minifi::core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  static constexpr auto Relationships = std::array<minifi::core::RelationshipDefinition, 1>{Success};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr minifi::core::annotation::Input InputRequirement = minifi::core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(Properties);
  }

  void onTrigger(minifi::core::ProcessContext& context, minifi::core::ProcessSession& session) override {
    auto flow_file = session.get();
    if (!flow_file) {
      flow_file = session.create();
    }
    std::string linked_service = context.getProperty("linkedService").value_or("");
    if (!IsNullOrEmpty(linked_service)) {
      std::shared_ptr<minifi::core::controller::ControllerServiceInterface> service = context.getControllerService(linked_service, getUUID());
      std::lock_guard<std::mutex> lock(control_mutex);
      REQUIRE(nullptr != service);
      REQUIRE("pushitrealgood" == std::dynamic_pointer_cast<MockControllerService>(service)->doSomething());
      // verify we have access to the controller service
      // and verify that we can execute it.
    }

    bool in_sub_process_group = context.getProperty("InSubProcessGroup") | minifi::utils::andThen(minifi::parsing::parseBool) | minifi::utils::orThrow("");
    auto sub_service = context.getControllerService("SubMockController", getUUID());
    if (in_sub_process_group) {
      REQUIRE(nullptr != sub_service);
      subprocess_controller_service_found_correctly = true;
    } else {
      REQUIRE(nullptr == sub_service);
      subprocess_controller_service_not_found_correctly = true;
    }
    session.transfer(flow_file, Success);
  }
};
