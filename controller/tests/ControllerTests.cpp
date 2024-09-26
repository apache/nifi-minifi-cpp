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
#include <fstream>
#include "range/v3/algorithm/find.hpp"

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Processor.h"
#include "Controller.h"
#include "c2/ControllerSocketProtocol.h"
#include "unit/TestUtils.h"
#include "c2/ControllerSocketMetricsPublisher.h"
#include "core/controller/ControllerServiceProvider.h"
#include "controllers/SSLContextService.h"
#include "utils/StringUtils.h"
#include "state/UpdateController.h"
#include "core/state/nodes/ResponseNodeLoader.h"

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

class TestBackTrace : public BackTrace {
 public:
  using BackTrace::BackTrace;
  void addTraceLines(uint32_t line_count) {
    for (uint32_t i = 1; i <= line_count; ++i) {
      addLine("bt line " + std::to_string(i) + " for " + getName());
    }
  }
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
    TestBackTrace trace1("trace1");
    trace1.addTraceLines(2);
    traces.push_back(trace1);
    TestBackTrace trace2("trace2");
    trace2.addTraceLines(3);
    traces.push_back(trace2);
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
    update_calls++;
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

class TestControllerServiceProvider : public core::controller::ControllerServiceProviderImpl {
 public:
  explicit TestControllerServiceProvider(std::shared_ptr<controllers::SSLContextService> ssl_context_service)
    : core::controller::ControllerServiceProviderImpl("TestControllerServiceProvider"),
      ssl_context_service_(std::move(ssl_context_service)) {
  }
  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string&) const override {
    return is_ssl_ ? ssl_context_service_ : nullptr;
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

  void setSsl() {
    is_ssl_ = true;
  }

 private:
  bool is_ssl_{};
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
    : configuration_(std::make_shared<minifi::ConfigureImpl>()),
      controller_(std::make_shared<TestStateController>()),
      update_sink_(std::make_unique<TestUpdateSink>(controller_)) {
    configuration_->set(minifi::Configure::controller_socket_host, "localhost");
    configuration_->set(minifi::Configure::controller_socket_port, "9997");
    configuration_->set(minifi::Configure::nifi_security_client_certificate, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "minifi-cpp-flow.crt").string());
    configuration_->set(minifi::Configure::nifi_security_client_private_key, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "minifi-cpp-flow.key").string());
    configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, "abcdefgh");
    configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "root-ca.pem").string());
    configuration_->set(minifi::Configure::controller_ssl_context_service, "SSLContextService");
    ssl_context_service_ = std::make_shared<controllers::SSLContextServiceImpl>("SSLContextService", configuration_);
    ssl_context_service_->onEnable();
    controller_service_provider_ = std::make_unique<TestControllerServiceProvider>(ssl_context_service_);
    controller_socket_data_.host = "localhost";
    controller_socket_data_.port = 9997;
  }

  void initalizeControllerSocket(const std::shared_ptr<c2::ControllerSocketReporter>& reporter = nullptr) {
    if (connection_type_ == ConnectionType::SSL_FROM_CONFIGURATION) {
      configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
    }
    controller_socket_protocol_ = std::make_unique<minifi::c2::ControllerSocketProtocol>(*controller_service_provider_, *update_sink_, configuration_, reporter);
    if (connection_type_ == ConnectionType::SSL_FROM_SERVICE_PROVIDER) {
      controller_service_provider_->setSsl();
    }
    controller_socket_protocol_->initialize();
  }

  void setConnectionType(ConnectionType connection_type) {
    connection_type_ = connection_type;
    if (connection_type_ == ConnectionType::UNSECURE) {
      controller_socket_data_.ssl_context_service = nullptr;
    } else {
      controller_socket_data_.ssl_context_service = ssl_context_service_;
    }
  }

 protected:
  ConnectionType connection_type_ = ConnectionType::UNSECURE;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<TestStateController> controller_;
  std::unique_ptr<TestUpdateSink> update_sink_;
  std::unique_ptr<minifi::c2::ControllerSocketProtocol> controller_socket_protocol_;
  std::shared_ptr<controllers::SSLContextService> ssl_context_service_;
  std::unique_ptr<TestControllerServiceProvider> controller_service_provider_;
  minifi::utils::net::SocketData controller_socket_data_;
};

TEST_CASE_METHOD(ControllerTestFixture, "Test listComponents", "[controllerTests]") {
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

  minifi::controller::startComponent(controller_socket_data_, "TestStateController");

  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return controller_->isRunning(); }, 20ms));

  minifi::controller::stopComponent(controller_socket_data_, "TestStateController");

  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return !controller_->isRunning(); }, 20ms));

  std::stringstream ss;
  minifi::controller::listComponents(controller_socket_data_, ss);

  REQUIRE(ss.str() == "Components:\nTestStateController, running: false\n");
}

TEST_CASE_METHOD(ControllerTestFixture, "TestClear", "[controllerTests]") {
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

  minifi::controller::startComponent(controller_socket_data_, "TestStateController");

  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return controller_->isRunning(); }, 20ms));

  for (auto i = 0; i < 3; ++i) {
    minifi::controller::clearConnection(controller_socket_data_, "connection");
  }

  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return 3 == update_sink_->clear_calls; }, 20ms));
}

TEST_CASE_METHOD(ControllerTestFixture, "TestUpdate", "[controllerTests]") {
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
  minifi::controller::startComponent(controller_socket_data_, "TestStateController");

  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return controller_->isRunning(); }, 20ms));

  std::stringstream ss;
  minifi::controller::updateFlow(controller_socket_data_, ss, "connection");

  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return 1 == update_sink_->update_calls; }, 20ms));
  REQUIRE(0 == update_sink_->clear_calls);
}

TEST_CASE_METHOD(ControllerTestFixture, "Test connection getters on empty flow", "[controllerTests]") {
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

  {
    std::stringstream connection_stream;
    minifi::controller::getConnectionSize(controller_socket_data_, connection_stream, "con1");
    CHECK(connection_stream.str() == "Size/Max of con1 not found\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::getFullConnections(controller_socket_data_, connection_stream);
    CHECK(connection_stream.str() == "0 are full\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::listConnections(controller_socket_data_, connection_stream);
    CHECK(connection_stream.str() == "Connection Names:\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::listConnections(controller_socket_data_, connection_stream, false);
    CHECK(connection_stream.str().empty());
  }
}

TEST_CASE_METHOD(ControllerTestFixture, "Test connection getters", "[controllerTests]") {
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

  {
    std::stringstream connection_stream;
    minifi::controller::getConnectionSize(controller_socket_data_, connection_stream, "conn");
    CHECK(connection_stream.str() == "Size/Max of conn not found\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::getConnectionSize(controller_socket_data_, connection_stream, "con1");
    CHECK(connection_stream.str() == "Size/Max of con1 1 / 2\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::getFullConnections(controller_socket_data_, connection_stream);
    CHECK(connection_stream.str() == "1 are full\ncon2 is full\n");
  }

  {
    std::stringstream connection_stream;
    minifi::controller::listConnections(controller_socket_data_, connection_stream);
    auto lines = minifi::utils::string::splitRemovingEmpty(connection_stream.str(), "\n");
    CHECK(lines.size() == 3);
    CHECK(ranges::find(lines, "Connection Names:") != ranges::end(lines));
    CHECK(ranges::find(lines, "con1") != ranges::end(lines));
    CHECK(ranges::find(lines, "con2") != ranges::end(lines));
  }

  {
    std::stringstream connection_stream;
    minifi::controller::listConnections(controller_socket_data_, connection_stream, false);
    auto lines = minifi::utils::string::splitRemovingEmpty(connection_stream.str(), "\n");
    CHECK(lines.size() == 2);
    CHECK(ranges::find(lines, "con1") != ranges::end(lines));
    CHECK(ranges::find(lines, "con2") != ranges::end(lines));
  }
}

TEST_CASE_METHOD(ControllerTestFixture, "Test manifest getter", "[controllerTests]") {
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
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  std::stringstream manifest_stream;
  minifi::controller::printManifest(controller_socket_data_, manifest_stream);
  REQUIRE(manifest_stream.str().find("\"agentType\": \"cpp\",") != std::string::npos);
}

TEST_CASE_METHOD(ControllerTestFixture, "Test jstack getter", "[controllerTests]") {
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
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  std::stringstream jstack_stream;
  minifi::controller::getJstacks(controller_socket_data_, jstack_stream);
  std::string expected_trace = "trace1 -- bt line 1 for trace1\n"
    "trace1 -- bt line 2 for trace1\n"
    "trace2 -- bt line 1 for trace2\n"
    "trace2 -- bt line 2 for trace2\n"
    "trace2 -- bt line 3 for trace2\n\n";
  REQUIRE(jstack_stream.str() == expected_trace);
}

TEST_CASE_METHOD(ControllerTestFixture, "Test debug bundle getter", "[controllerTests]") {
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
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  TestController test_controller;
  auto output_dir = test_controller.createTempDirectory();
  REQUIRE(minifi::controller::getDebugBundle(controller_socket_data_, output_dir));
  REQUIRE(std::filesystem::exists(output_dir / "debug.tar.gz"));
}

TEST_CASE_METHOD(ControllerTestFixture, "Test debug bundle is created to non-existent folder", "[controllerTests]") {
  setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);

  auto reporter = std::make_shared<minifi::c2::ControllerSocketMetricsPublisher>("ControllerSocketMetricsPublisher");
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  TestController test_controller;
  auto output_dir = test_controller.createTempDirectory() / "subfolder";
  REQUIRE(minifi::controller::getDebugBundle(controller_socket_data_, output_dir));
  REQUIRE(std::filesystem::exists(output_dir / "debug.tar.gz"));
}

TEST_CASE_METHOD(ControllerTestFixture, "Debug bundle retrieval fails if target path is an existing file", "[controllerTests]") {
  setConnectionType(ControllerTestFixture::ConnectionType::UNSECURE);

  auto reporter = std::make_shared<minifi::c2::ControllerSocketMetricsPublisher>("ControllerSocketMetricsPublisher");
  auto response_node_loader = std::make_shared<minifi::state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  reporter->initialize(configuration_, response_node_loader);
  initalizeControllerSocket(reporter);

  TestController test_controller;
  auto invalid_path = test_controller.createTempDirectory() / "test.log";
  std::ofstream file(invalid_path);
  auto result = minifi::controller::getDebugBundle(controller_socket_data_, invalid_path);
  REQUIRE(!result);
  REQUIRE(result.error() == "Object specified as the target directory already exists and it is not a directory");
}

}  // namespace org::apache::nifi::minifi::test
