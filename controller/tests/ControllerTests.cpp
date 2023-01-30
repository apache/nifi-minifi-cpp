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
#include <filesystem>
#include "TestBase.h"
#include "Catch.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "Controller.h"
#include "c2/ControllerSocketProtocol.h"
#include "utils/IntegrationTestUtils.h"
#include "c2/ControllerSocketMetricsPublisher.h"
#include "core/controller/ControllerServiceProvider.h"
#include "controllers/SSLContextService.h"

#include "state/UpdateController.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class TestStateController : public minifi::state::StateController {
 public:
  TestStateController()
    : is_running(false) {
  }

  std::string getComponentName() const override {
    return "TestStateController";
  }

  minifi::utils::Identifier getComponentUUID() const override {
    static auto dummyUUID = minifi::utils::Identifier::parse("12345678-1234-1234-1234-123456789abc").value();
    return dummyUUID;
  }

  int16_t start() override {
    is_running = true;
    return 0;
  }

  int16_t stop() override {
    is_running = false;
    return 0;
  }

  bool isRunning() const override {
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

  minifi::utils::Identifier getComponentUUID() const override {
    static auto dummyUUID = minifi::utils::Identifier::parse("12345678-1234-1234-1234-123456789abc").value();
    return dummyUUID;
  }

  int16_t start() override {
    is_running = true;
    return 0;
  }

  int16_t stop() override {
    is_running = false;
    return 0;
  }

  bool isRunning() const override {
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

  int16_t drainRepositories() override {
    return 0;
  }

  std::map<std::string, std::unique_ptr<minifi::io::InputStream>> getDebugInfo() override {
    return {};
  }

  int16_t clearConnection(const std::string& /*connection*/) override {
    clear_calls++;
    return 0;
  }

  std::vector<std::string> getSupportedConfigurationFormats() const override {
    return {};
  }

  int16_t applyUpdate(const std::string& /*source*/, const std::string& /*configuration*/, bool /*persist*/ = false, const std::optional<std::string>& /*flow_id*/ = std::nullopt) override {
    update_calls++;
    return 0;
  }

  int16_t applyUpdate(const std::string& /*source*/, const std::shared_ptr<minifi::state::Update>& /*updateController*/) override {
    return 0;
  }

  uint64_t getUptime() override {
    return 8765309;
  }

  std::atomic<bool> is_running;
  std::atomic<uint32_t> clear_calls;
  std::shared_ptr<StateController> controller;
  std::atomic<uint32_t> update_calls;
};

class TestControllerSocketReporter : public c2::ControllerSocketReporter {
  std::unordered_map<std::string, ControllerSocketReporter::QueueSize> getQueueSizes() override {
    return {
      {"con1", {1, 2}},
      {"con2", {3, 3}}
    };
  }

  std::unordered_set<std::string> getFullConnections() override {
    return {"con2"};
  }

  std::unordered_set<std::string> getConnections() override {
    return {"con1", "con2"};
  }

  std::string getAgentManifest() override {
    return "testAgentManifest";
  }
};

class TestControllerServiceProvider : public core::controller::ControllerServiceProvider {
 public:
  explicit TestControllerServiceProvider(std::shared_ptr<controllers::SSLContextService> ssl_context_service)
    : core::controller::ControllerServiceProvider("TestControllerServiceProvider"),
      ssl_context_service_(std::move(ssl_context_service)) {
  }
  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string&) const override {
    return ssl_context_service_;
  }

  std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(const std::string&, const std::string&, const std::string&, bool) override {
    return nullptr;
  }
  void clearControllerServices() override {
  }
  void enableAllControllerServices() override {
  }
  void disableAllControllerServices() override {
  }

 private:
  std::shared_ptr<controllers::SSLContextService> ssl_context_service_;
};

class ControllerTestFixture {
 public:
  enum class ConnectionType {
    UNSECURE,
    SSL_FROM_SERVICE_PROVIDER,
    SSL_FROM_CONFIGURATION
  };

  ControllerTestFixture()
    : configuration_(std::make_shared<minifi::Configure>()),
      controller_(std::make_shared<TestStateController>()),
      update_sink_(std::make_unique<TestUpdateSink>(controller_)),
      stream_factory_(minifi::io::StreamFactory::getInstance(configuration_)) {
    configuration_->set(minifi::Configure::controller_socket_host, "localhost");
    configuration_->set(minifi::Configure::controller_socket_port, "9997");
    configuration_->set(minifi::Configure::nifi_security_client_certificate, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "minifi-cpp-flow.crt").string());
    configuration_->set(minifi::Configure::nifi_security_client_private_key, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "minifi-cpp-flow.key").string());
    configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, "abcdefgh");
    configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "root-ca.pem").string());
    configuration_->set(minifi::Configure::controller_ssl_context_service, "SSLContextService");
    ssl_context_service_ = std::make_shared<controllers::SSLContextService>("SSLContextService", configuration_);
    ssl_context_service_->onEnable();
    controller_service_provider_ = std::make_unique<TestControllerServiceProvider>(ssl_context_service_);
  }

  void initalizeControllerSocket(const std::shared_ptr<c2::ControllerSocketReporter>& reporter = nullptr) {
    if (connection_type_ == ConnectionType::SSL_FROM_CONFIGURATION) {
      configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
    }
    controller_socket_protocol_ = std::make_unique<minifi::c2::ControllerSocketProtocol>(
      connection_type_ == ConnectionType::SSL_FROM_SERVICE_PROVIDER ? controller_service_provider_.get() : nullptr, update_sink_.get(), configuration_, reporter);
    controller_socket_protocol_->initialize();
  }

  std::unique_ptr<minifi::io::Socket> createSocket() {
    if (connection_type_ == ConnectionType::UNSECURE) {
      return stream_factory_->createSocket("localhost", 9997);
    } else {
      return stream_factory_->createSecureSocket("localhost", 9997, ssl_context_service_);
    }
  }

  void setConnectionType(ConnectionType connection_type) {
    connection_type_ = connection_type;
  }

 protected:
  ConnectionType connection_type_ = ConnectionType::UNSECURE;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<TestStateController> controller_;
  std::unique_ptr<TestUpdateSink> update_sink_;
  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;
  std::unique_ptr<minifi::c2::ControllerSocketProtocol> controller_socket_protocol_;
  std::shared_ptr<controllers::SSLContextService> ssl_context_service_;
  std::unique_ptr<TestControllerServiceProvider> controller_service_provider_;
};

#ifdef WIN32
TEST_CASE("TestWindows", "[test1]") {
  std::cout << "Controller Tests are not supported on windows";
}
#else
TEST_CASE_METHOD(ControllerTestFixture, "TestGet", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  initalizeControllerSocket();

  auto socket = createSocket();

  minifi::controller::startComponent(std::move(socket), "TestStateController");

  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return controller_->isRunning(); }, 20ms));

  socket = createSocket();

  minifi::controller::stopComponent(std::move(socket), "TestStateController");

  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return !controller_->isRunning(); }, 20ms));

  socket = createSocket();
  std::stringstream ss;
  minifi::controller::listComponents(std::move(socket), ss);

  REQUIRE(ss.str().find("TestStateController") != std::string::npos);
}

TEST_CASE_METHOD(ControllerTestFixture, "TestClear", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  initalizeControllerSocket();

  auto socket = createSocket();

  minifi::controller::startComponent(std::move(socket), "TestStateController");

  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return controller_->isRunning(); }, 20ms));

  socket = createSocket();

  minifi::controller::clearConnection(std::move(socket), "connection");

  socket = createSocket();

  minifi::controller::clearConnection(std::move(socket), "connection");

  socket = createSocket();

  minifi::controller::clearConnection(std::move(socket), "connection");

  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return 3 == update_sink_->clear_calls; }, 20ms));
}

TEST_CASE_METHOD(ControllerTestFixture, "TestUpdate", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  initalizeControllerSocket();

  auto socket = createSocket();

  minifi::controller::startComponent(std::move(socket), "TestStateController");

  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return controller_->isRunning(); }, 20ms));

  std::stringstream ss;

  socket = createSocket();

  minifi::controller::updateFlow(std::move(socket), ss, "connection");

  REQUIRE(verifyEventHappenedInPollTime(500ms, [&] { return 1 == update_sink_->update_calls; }, 20ms));
  REQUIRE(0 == update_sink_->clear_calls);
}

TEST_CASE_METHOD(ControllerTestFixture, "Test connection getters on empty flow", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  initalizeControllerSocket();

  auto socket = createSocket();

  std::stringstream connection_stream;
  minifi::controller::getConnectionSize(std::move(socket), connection_stream, "con1");
  CHECK(connection_stream.str() == "Size/Max of con1 not found\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::getFullConnections(std::move(socket), connection_stream);
  CHECK(connection_stream.str() == "0 are full\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::listConnections(std::move(socket), connection_stream);
  CHECK(connection_stream.str() == "Connection Names:\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::listConnections(std::move(socket), connection_stream, false);
  CHECK(connection_stream.str().empty());
}

TEST_CASE_METHOD(ControllerTestFixture, "Test connection getters", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  auto reporter = std::make_shared<TestControllerSocketReporter>();
  initalizeControllerSocket(reporter);

  std::stringstream connection_stream;
  auto socket = createSocket();
  minifi::controller::getConnectionSize(std::move(socket), connection_stream, "conn");
  CHECK(connection_stream.str() == "Size/Max of conn not found\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::getConnectionSize(std::move(socket), connection_stream, "con1");
  CHECK(connection_stream.str() == "Size/Max of con1 1 / 2\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::getFullConnections(std::move(socket), connection_stream);
  CHECK(connection_stream.str() == "1 are full\ncon2 is full\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::listConnections(std::move(socket), connection_stream);
  CHECK(connection_stream.str() == "Connection Names:\ncon2\ncon1\n");

  connection_stream.str(std::string());
  socket = createSocket();
  minifi::controller::listConnections(std::move(socket), connection_stream, false);
  CHECK(connection_stream.str() == "con2\ncon1\n");
}

TEST_CASE_METHOD(ControllerTestFixture, "Test manifest getter", "[test1]") {
  SECTION("With SSL from service provider") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_SERVICE_PROVIDER);
  }

  SECTION("With SSL from properties") {
    setConnectionType(ControllerTestFixture::ConnectionType::SSL_FROM_CONFIGURATION);
  }

  SECTION("Without SSL") {
    setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);
  }

  auto reporter = std::make_shared<minifi::c2::ControllerSocketMetricsPublisher>("ControllerSocketMetricsPublisher");
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoader>(configuration_, nullptr, nullptr, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  std::stringstream manifest_stream;
  auto socket = createSocket();
  minifi::controller::printManifest(std::move(socket), manifest_stream);
  REQUIRE(manifest_stream.str().find("\"agentType\": \"cpp\",") != std::string::npos);
}
#endif

}  // namespace org::apache::nifi::minifi::test
