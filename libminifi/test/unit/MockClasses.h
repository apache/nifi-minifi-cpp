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

#include <set>
#include <memory>
#include <string>

#include "core/controller/ControllerService.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

std::atomic<bool> disabled;
std::mutex control_mutex;

class MockControllerService : public core::controller::ControllerService {
 public:
  explicit MockControllerService(const std::string &name, const utils::Identifier &uuid)
      : ControllerService(name, uuid) {
  }

  explicit MockControllerService(const std::string &name)
      : ControllerService(name) {
  }
  MockControllerService() = default;

  ~MockControllerService() = default;

  virtual void initialize() {
    core::controller::ControllerService::initialize();
    enable();
  }

  std::string doSomething() {
    return str;
  }

  virtual void enable() {
    str = "pushitrealgood";
  }

  void yield() {
  }

  bool isRunning() {
    return true;
  }

  bool isWorkAvailable() {
    return true;
  }

 protected:
  std::string str;
};

class MockProcessor : public core::Processor {
 public:
  explicit MockProcessor(const std::string &name, const utils::Identifier &uuid)
      : Processor(name, uuid) {
    setTriggerWhenEmpty(true);
  }

  explicit MockProcessor(const std::string &name)
      : Processor(name) {
    setTriggerWhenEmpty(true);
  }

  ~MockProcessor() = default;

  void initialize() override {
    core::Property property("linkedService", "Linked service");
    std::set<core::Property> properties;
    properties.insert(property);
    setSupportedProperties(properties);
  }

  // OnTrigger method, implemented by NiFi Processor Designer
  void onTrigger(core::ProcessContext *context, core::ProcessSession* /*session*/) override {
    std::string linked_service = "";
    getProperty("linkedService", linked_service);
    if (!IsNullOrEmpty(linked_service)) {
      std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(linked_service);
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
