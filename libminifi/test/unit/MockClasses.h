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

namespace minifi = org::apache::nifi::minifi;

std::atomic<bool> disabled;
std::mutex control_mutex;

class MockControllerService : public minifi::core::controller::ControllerService {
 public:
  explicit MockControllerService(std::string name, const minifi::utils::Identifier &uuid)
      : ControllerService(std::move(name), uuid) {
  }

  explicit MockControllerService(std::string name)
      : ControllerService(std::move(name)) {
  }
  MockControllerService() = default;

  ~MockControllerService() override = default;

  static constexpr const char* Description = "An example service";
  static auto properties() { return std::array<minifi::core::Property, 0>{}; }
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {
    minifi::core::controller::ControllerService::initialize();
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

  bool isRunning() override {
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
  explicit MockProcessor(std::string name, const minifi::utils::Identifier &uuid)
      : Processor(std::move(name), uuid) {
    setTriggerWhenEmpty(true);
  }

  explicit MockProcessor(std::string name)
      : Processor(std::move(name)) {
    setTriggerWhenEmpty(true);
  }

  ~MockProcessor() override = default;

  static constexpr const char* Description = "An example processor";
  static inline const minifi::core::Property LinkedService{"linkedService", "Linked service"};
  static auto properties() { return std::array{LinkedService}; }
  static auto relationships() { return std::array<minifi::core::Relationship, 0>{}; }
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr minifi::core::annotation::Input InputRequirement = minifi::core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(properties());
  }

  void onTrigger(minifi::core::ProcessContext *context, minifi::core::ProcessSession* /*session*/) override {
    std::string linked_service = "";
    getProperty("linkedService", linked_service);
    if (!IsNullOrEmpty(linked_service)) {
      std::shared_ptr<minifi::core::controller::ControllerService> service = context->getControllerService(linked_service);
      std::lock_guard<std::mutex> lock(control_mutex);
      if (!disabled.load()) {
        assert(true == context->isControllerServiceEnabled(linked_service));
        assert(nullptr != service);
        assert("pushitrealgood" == std::static_pointer_cast<MockControllerService>(service)->doSomething());
      } else {
        assert(false == context->isControllerServiceEnabled(linked_service));
      }
      // verify we have access to the controller service
      // and verify that we can execute it.
    }
  }

  bool isYield() override {
    return false;
  }
};
