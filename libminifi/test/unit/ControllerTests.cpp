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
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "../TestBase.h"
#include "../Catch.h"
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

  std::string getComponentName() const override {
    return "TestStateController";
  }

  utils::Identifier getComponentUUID() const override {
    static auto dummyUUID = utils::Identifier::parse("12345678-1234-1234-1234-123456789abc").value();
    return dummyUUID;
  }

  /**
   * Start the client
   */
  int16_t start() override {
    is_running = true;
    return 0;
  }
  /**
   * Stop the client
   */
  int16_t stop() override {
    is_running = false;
    return 0;
  }

  bool isRunning() override {
    return is_running;
  }

  int16_t pause() override {
    return 0;
  }

  int16_t resume() override {
    return 0;
  }

  std::atomic<bool> is_running;
};

class TestUpdateSink : public minifi::state::StateMonitor {
 public:
  explicit TestUpdateSink(std::shared_ptr<StateController> controller)
      : is_running(true),
        clear_calls(0),
        controller(std::move(controller)),
        update_calls(0) {
  }

  void executeOnComponent(const std::string&, std::function<void(minifi::state::StateController&)> func) override {
    func(*controller);
  }

  void executeOnAllComponents(std::function<void(minifi::state::StateController&)> func) override {
    func(*controller);
  }

  std::string getComponentName() const override {
    return "TestUpdateSink";
  }

  utils::Identifier getComponentUUID() const override {
    static auto dummyUUID = utils::Identifier::parse("12345678-1234-1234-1234-123456789abc").value();
    return dummyUUID;
  }
  /**
   * Start the client
   */
  int16_t start() override {
    is_running = true;
    return 0;
  }
  /**
   * Stop the client
   */
  int16_t stop() override {
    is_running = false;
    return 0;
  }

  bool isRunning() override {
    return is_running;
  }

  int16_t pause() override {
    return 0;
  }

  int16_t resume() override {
    return 0;
  }
  std::vector<BackTrace> getTraces() override {
    std::vector<BackTrace> traces;
    return traces;
  }

  /**
   * Operational controllers
   */

  /**
   * Drain repositories
   */
  int16_t drainRepositories() override {
    return 0;
  }

  std::map<std::string, std::unique_ptr<minifi::io::InputStream>> getDebugInfo() override {
    return {};
  }

  /**
   * Clear connection for the agent.
   */
  int16_t clearConnection(const std::string& /*connection*/) override {
    clear_calls++;
    return 0;
  }

  /**
   * Apply an update with the provided string.
   *
   * < 0 is an error code
   * 0 is success
   */
  int16_t applyUpdate(const std::string& /*source*/, const std::string& /*configuration*/, bool /*persist*/ = false) override {
    update_calls++;
    return 0;
  }

  /**
   * Apply an update that the agent must decode. This is useful for certain operations
   * that can't be encapsulated within these definitions.
   */
  int16_t applyUpdate(const std::string& /*source*/, const std::shared_ptr<minifi::state::Update>& /*updateController*/) override {
    return 0;
  }

  /**
   * Returns uptime for this module.
   * @return uptime for the current state monitor.
   */
  uint64_t getUptime() override {
    return 8765309;
  }

  std::atomic<bool> is_running;
  std::atomic<uint32_t> clear_calls;
  std::shared_ptr<StateController> controller;
  std::atomic<uint32_t> update_calls;
};
#ifdef WIN32
TEST_CASE("TestWindows", "[test1]") {
  std::cout << "Controller Tests are not supported on windows";
}
#else
TEST_CASE("TestGet", "[test1]") {
  auto controller = std::make_shared<TestStateController>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set("controller.socket.host", "localhost");
  configuration->set("controller.socket.port", "9997");
  auto ptr = std::make_unique<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr.get(), configuration);

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
  auto ptr = std::make_unique<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr.get(), configuration);

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
  auto ptr = std::make_unique<TestUpdateSink>(controller);
  minifi::c2::ControllerSocketProtocol protocol("testprotocol");
  protocol.initialize(nullptr, ptr.get(), configuration);

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
#endif
