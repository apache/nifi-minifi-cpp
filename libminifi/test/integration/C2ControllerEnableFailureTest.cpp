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
#include <string>

#include "unit/TestBase.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/Catch.h"
#include "core/Processor.h"
#include "core/controller/ControllerService.h"
#include "core/Resource.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class DummyController : public core::controller::ControllerServiceImpl {
 public:
  explicit DummyController(std::string_view name, const minifi::utils::Identifier &uuid = {}) : ControllerServiceImpl(name, uuid) {}

  static constexpr const char* Description = "Dummy Controller";

  static constexpr auto DummyControllerProperty = core::PropertyDefinitionBuilder<>::createProperty("Dummy Controller Property")
      .withDescription("Dummy Controller Property")
      .build();

  static constexpr auto Properties = std::to_array<core::PropertyReference>({DummyControllerProperty});
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {
    setSupportedProperties(Properties);
  }

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override {
    auto dummy_controller_property = getProperty(DummyControllerProperty.name);
    if (!dummy_controller_property || dummy_controller_property->empty()) {
      throw minifi::Exception(minifi::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing dummy property");
    }
  }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<DummyController>::getLogger(uuid_);
};

REGISTER_RESOURCE(DummyController, ControllerService);

class DummmyControllerUserProcessor : public minifi::core::ProcessorImpl {
  using minifi::core::ProcessorImpl::ProcessorImpl;

 public:
  DummmyControllerUserProcessor(std::string_view name, const minifi::utils::Identifier& uuid) : ProcessorImpl(name, uuid) {}
  explicit DummmyControllerUserProcessor(std::string_view name) : ProcessorImpl(name) {}
  static constexpr auto DummyControllerService = core::PropertyDefinitionBuilder<>::createProperty("Dummy Controller Service")
    .withDescription("Dummy Controller Service")
    .withAllowedTypes<DummyController>()
    .build();

  void initialize() override {
    setSupportedProperties(Properties);
  }

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& /*session_factory*/) override {
    if (auto controller_service = context.getProperty(DummmyControllerUserProcessor::DummyControllerService)) {
      if (!std::dynamic_pointer_cast<DummyController>(context.getControllerService(*controller_service, uuid_))) {
        throw minifi::Exception(minifi::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Invalid controller service");
      }
    } else {
      throw minifi::Exception(minifi::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing controller service");
    }
    logger_->log_debug("DummyControllerUserProcessor::onSchedule successful");
  }

  static constexpr const char* Description = "A processor that uses controller.";
  static constexpr auto Properties = std::array<core::PropertyReference, 1>{DummyControllerService};
  static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<DummmyControllerUserProcessor>::getLogger(uuid_);
};

REGISTER_RESOURCE(DummmyControllerUserProcessor, Processor);

class VerifyC2ControllerUpdate : public VerifyC2Base {
 public:
  explicit VerifyC2ControllerUpdate(const std::atomic_bool& flow_updated_successfully) : flow_updated_successfully_(flow_updated_successfully) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<DummmyControllerUserProcessor>();
    LogTestController::getInstance().setDebug<DummyController>();
    LogTestController::getInstance().setDebug<core::controller::StandardControllerServiceProvider>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
    REQUIRE(verifyEventHappenedInPollTime(40s, [&] { return flow_updated_successfully_.load(); }, 1s));
  }

 private:
  const std::atomic_bool& flow_updated_successfully_;
};

class ControllerUpdateHandler: public HeartbeatHandler {
 public:
  explicit ControllerUpdateHandler(std::atomic_bool& flow_updated_successfully, std::shared_ptr<minifi::Configure> configuration, const std::filesystem::path& replacement_config_path)
    : HeartbeatHandler(std::move(configuration)),
      flow_updated_successfully_(flow_updated_successfully),
      replacement_config_(minifi::utils::file::get_content(replacement_config_path.string())) {
  }

  void handleHeartbeat(const rapidjson::Document& /*root*/, struct mg_connection* conn) override {
    switch (test_state_) {
      case TestState::VERIFY_INITIAL_METRICS: {
        sendEmptyHeartbeatResponse(conn);
        REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(5s, "Could not enable DummyController"));
        REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(5s, "(DummmyControllerUserProcessor): Process Schedule Operation: Invalid controller service"));
        test_state_ = TestState::SEND_NEW_CONFIG;
        break;
      }
      case TestState::SEND_NEW_CONFIG: {
        sendHeartbeatResponse("UPDATE", "configuration", "889349", conn, {{"configuration_data", minifi::c2::C2Value{replacement_config_}}});
        test_state_ = TestState::VERIFY_UPDATED_METRICS;
        break;
      }
      case TestState::VERIFY_UPDATED_METRICS: {
        sendEmptyHeartbeatResponse(conn);
        if (minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "DummyControllerUserProcessor::onSchedule successful")) {
          flow_updated_successfully_ = true;
        }
        break;
      }
    }
  }

 private:
  enum class TestState {
    VERIFY_INITIAL_METRICS,
    SEND_NEW_CONFIG,
    VERIFY_UPDATED_METRICS
  };

  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  std::atomic_bool& flow_updated_successfully_;
  TestState test_state_ = TestState::VERIFY_INITIAL_METRICS;
  std::string replacement_config_;
};

TEST_CASE("C2ControllerEnableFailureTest", "[c2test]") {
  std::atomic_bool flow_updated_successfully{false};
  VerifyC2ControllerUpdate harness(flow_updated_successfully);
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestC2InvalidController.yml";
  auto replacement_path = test_file_path.string();
  minifi::utils::string::replaceAll(replacement_path, "TestC2InvalidController", "TestC2ValidController");
  ControllerUpdateHandler handler(flow_updated_successfully, harness.getConfiguration(), replacement_path);
  harness.setUrl("https://localhost:0/api/heartbeat", &handler);
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
