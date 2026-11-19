// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/controller/ControllerServiceBase.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/utils/gsl.h"

namespace minifi = org::apache::nifi::minifi;

std::mutex control_mutex;
std::atomic<bool> controller_service_action_is_correct{false};
std::atomic<bool> controller_service_action_failed{false};
std::atomic<bool> subprocess_controller_service_found_correctly{false};
std::atomic<bool> subprocess_controller_service_not_found_correctly{false};
std::atomic<bool> subprocess_controller_service_lookup_failed{false};

class MockControllerService : public minifi::core::controller::ControllerServiceBase, public core::controller::ControllerServiceHandle {
 public:
  using ControllerServiceBase::ControllerServiceBase;
  MockControllerService()
    : ControllerServiceBase({.uuid = utils::Identifier{}, .name = "MockControllerService", .logger = logging::LoggerFactory<MockControllerService>::getLogger()}) {}

  ~MockControllerService() override = default;

  static constexpr const char* Description = "An example service";
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {
    enable();
  }

  void onEnable() override {}

  std::string doSomething() {
    return str;
  }

  virtual void enable() {
    str = "pushitrealgood";
  }

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}

 protected:
  std::string str;
};

class MockProcessor : public minifi::core::ProcessorImpl {
 public:
  explicit MockProcessor(minifi::core::ProcessorMetadata metadata)
      : ProcessorImpl(metadata) {}

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

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) override {
    context.setTriggerWhenEmpty(true);
  }

  void onTrigger(minifi::core::ProcessContext& context, minifi::core::ProcessSession& session) override {
    auto flow_file = session.get();
    if (!flow_file) {
      flow_file = session.create();
    }
    std::string linked_service = context.getProperty("linkedService").value_or("");
    if (!IsNullOrEmpty(linked_service)) {
      std::shared_ptr<minifi::core::controller::ControllerServiceHandle> service = context.getControllerService(linked_service, getUUID());
      std::lock_guard<std::mutex> lock(control_mutex);
      // verify we have access to the controller service and we can execute it
      if (service && "pushitrealgood" == std::dynamic_pointer_cast<MockControllerService>(service)->doSomething()) {
        controller_service_action_is_correct = true;
      } else {
        controller_service_action_failed = true;
      }
    }

    bool in_sub_process_group = context.getProperty("InSubProcessGroup") | minifi::utils::andThen(minifi::parsing::parseBool) | minifi::utils::orThrow("");
    auto sub_service = context.getControllerService("SubMockController", getUUID());
    if (sub_service && in_sub_process_group) {
      subprocess_controller_service_found_correctly = true;
    } else if (!sub_service && !in_sub_process_group) {
      subprocess_controller_service_not_found_correctly = true;
    } else {
      subprocess_controller_service_lookup_failed = true;
    }
    session.transfer(flow_file, Success);
  }
};
