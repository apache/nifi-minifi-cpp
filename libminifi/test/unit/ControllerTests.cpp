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
#include <uuid/uuid.h>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "../TestBase.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "../../controller/Controller.h"
#include "c2/ControllerSocketProtocol.h"

#include "state/UpdateController.h"

class TestStateController : public minifi::state::StateController {
 public:
  TestStateController()
      : is_running(false) {
  }
  virtual ~TestStateController() {
  }

  virtual std::string getComponentName() const {
    return "TestStateController";
  }

  virtual std::string getComponentUUID() const {
    return "uuid";
  }

  /**
   * Start the client
   */
  virtual int16_t start() {
    is_running = true;
    return 0;
  }
  /**
   * Stop the client
   */
  virtual int16_t stop(bool force, uint64_t timeToWait = 0) {
    is_running = false;
    return 0;
  }

  virtual bool isRunning() {
    return is_running;
  }

  virtual int16_t pause() {
    return 0;
  }

  std::atomic<bool> is_running;
};

class TestUpdateSink : public minifi::state::StateMonitor {
 public:
  explicit TestUpdateSink(std::shared_ptr<StateController> controller)
      : is_running(true),
        clear_calls(0),
        controller(controller),
        update_calls(0) {
  }
  virtual std::vector<std::shared_ptr<StateController>> getComponents(const std::string &name) {
    std::vector<std::shared_ptr<StateController>> vec;
    vec.push_back(controller);
    return vec;
  }

  virtual std::vector<std::shared_ptr<StateController>> getAllComponents() {
    std::vector<std::shared_ptr<StateController>> vec;
    vec.push_back(controller);
    return vec;
  }

  virtual std::string getComponentName() const {
    return "TestUpdateSink";
  }

  virtual std::string getComponentUUID() const {
      return "uuid";
    }
  /**
   * Start the client
   */
  virtual int16_t start() {
    is_running = true;
    return 0;
  }
  /**
   * Stop the client
   */
  virtual int16_t stop(bool force, uint64_t timeToWait = 0) {
    is_running = false;
    return 0;
  }

  virtual bool isRunning() {
    return is_running;
  }

  virtual int16_t pause() {
    return 0;
  }
  virtual std::vector<BackTrace> getTraces() {
    std::vector<BackTrace> traces;
    return traces;
  }

  /**
   * Operational controllers
   */

  /**
   * Drain repositories
   */
  virtual int16_t drainRepositories() {
    return 0;
  }

  /**
   * Clear connection for the agent.
   */
  virtual int16_t clearConnection(const std::string &connection) {
    clear_calls++;
    return 0;
  }

  /**
   * Apply an update with the provided string.
   *
   * < 0 is an error code
   * 0 is success
   */
  virtual int16_t applyUpdate(const std::string &source, const std::string &configuration) {
    update_calls++;
    return 0;
  }

  /**
   * Apply an update that the agent must decode. This is useful for certain operations
   * that can't be encapsulated within these definitions.
   */
  virtual int16_t applyUpdate(const std::string &source, const std::shared_ptr<minifi::state::Update> &updateController) {
    return 0;
  }

  /**
   * Returns uptime for this module.
   * @return uptime for the current state monitor.
   */
  virtual uint64_t getUptime() {
    return 8765309;
  }

  std::atomic<bool> is_running;
  std::atomic<uint32_t> clear_calls;
  std::shared_ptr<StateController> controller;
  std::atomic<uint32_t> update_calls;
};

TEST_CASE("TestGet", "[test1]") {
  auto controller = std::make_shared<TestStateController>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set("controller.socket.host", "localhost");
  configuration->set("controller.socket.port", "9997");
  auto ptr = std::make_shared<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr, configuration);

  auto stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  auto socket = stream_factory->createSocket("localhost", 9997);

  startComponent(std::move(socket), "TestStateController");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  REQUIRE(controller->isRunning() == true);

  socket = stream_factory->createSocket("localhost", 9997);

  stopComponent(std::move(socket), "TestStateController");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  REQUIRE(controller->isRunning() == false);

  socket = stream_factory->createSocket("localhost", 9997);
  std::stringstream ss;
  listComponents(std::move(socket), ss);

  REQUIRE(ss.str().find("TestStateController") != std::string::npos);
}

TEST_CASE("TestClear", "[test1]") {
  auto controller = std::make_shared<TestStateController>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set("controller.socket.host", "localhost");
  configuration->set("controller.socket.port", "9997");
  auto ptr = std::make_shared<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr, configuration);

  auto stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  auto socket = stream_factory->createSocket("localhost", 9997);

  startComponent(std::move(socket), "TestStateController");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  REQUIRE(controller->isRunning() == true);

  socket = stream_factory->createSocket("localhost", 9997);

  clearConnection(std::move(socket), "connection");

  socket = stream_factory->createSocket("localhost", 9997);

  clearConnection(std::move(socket), "connection");

  socket = stream_factory->createSocket("localhost", 9997);

  clearConnection(std::move(socket), "connection");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  REQUIRE(3 == ptr->clear_calls);
}

TEST_CASE("TestUpdate", "[test1]") {
  auto controller = std::make_shared<TestStateController>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set("controller.socket.host", "localhost");
  configuration->set("controller.socket.port", "9997");
  auto ptr = std::make_shared<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr, configuration);

  auto stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  auto socket = stream_factory->createSocket("localhost", 9997);

  startComponent(std::move(socket), "TestStateController");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  REQUIRE(controller->isRunning() == true);

  std::stringstream ss;

  socket = stream_factory->createSocket("localhost", 9997);

  updateFlow(std::move(socket), ss, "connection");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  REQUIRE(1 == ptr->update_calls);
  REQUIRE(0 == ptr->clear_calls);
}
