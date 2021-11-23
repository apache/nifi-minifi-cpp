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

#undef NDEBUG
#include <string>
#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"

using namespace std::literals::chrono_literals;

class VerifyC2ClearCoreComponentState : public VerifyC2Base {
 public:
  explicit VerifyC2ClearCoreComponentState(const std::atomic_bool& component_cleared_successfully) : component_cleared_successfully_(component_cleared_successfully) {
    auto temp_dir = testController.createTempDirectory();
    test_file_1_ = minifi::utils::putFileToDir(temp_dir, "test1.txt", "foo\n");
    test_file_2_ = minifi::utils::putFileToDir(temp_dir, "test2.txt", "foobar\n");
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessContext>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(40s, [&] { return component_cleared_successfully_.load(); }));
  }

 protected:
  void updateProperties(minifi::FlowController& flow_controller) override {
    dynamic_cast<minifi::state::ProcessorController*>(flow_controller.getComponents("TailFile1")[0])
        ->getProcessor()->setProperty(minifi::processors::TailFile::FileName, test_file_1_);
    dynamic_cast<minifi::state::ProcessorController*>(flow_controller.getComponents("TailFile2")[0])
        ->getProcessor()->setProperty(minifi::processors::TailFile::FileName, test_file_2_);
  }

  TestController testController;
  std::string test_file_1_;
  std::string test_file_2_;
  const std::atomic_bool& component_cleared_successfully_;
};

class ClearCoreComponentStateHandler: public HeartbeatHandler {
 public:
  explicit ClearCoreComponentStateHandler(std::atomic_bool& component_cleared_successfully) : component_cleared_successfully_(component_cleared_successfully) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    switch (flow_state_) {
      case FlowState::STARTED:
        sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889345", conn);
        flow_state_ = FlowState::FIRST_DESCRIBE_SENT;
        break;
      case FlowState::FIRST_DESCRIBE_SENT: {
        sendHeartbeatResponse("CLEAR", "corecomponentstate", "889346", conn, { {"corecomponent1", "TailFile1"} });
        flow_state_ = FlowState::CLEAR_SENT;
        break;
      }
      default:
        sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889347", conn);
        flow_state_ = FlowState::SECOND_DESCRIBE_SENT;
    }
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    switch (flow_state_) {
      case FlowState::FIRST_DESCRIBE_SENT: {
        assert(root.HasMember("corecomponentstate"));

        auto assertExpectedTailFileState = [&](const char* uuid, const char* name, const char* position) {
          assert(root["corecomponentstate"].HasMember(uuid));
          const auto& tf = root["corecomponentstate"][uuid];
          assert(tf.HasMember("file.0.name"));
          assert(std::string(tf["file.0.name"].GetString()) == name);
          assert(tf.HasMember("file.0.position"));
          assert(std::string(tf["file.0.position"].GetString()) == position);
          assert(tf.HasMember("file.0.current"));
          assert(strlen(tf["file.0.current"].GetString()) > 0U);
        };

        assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1993", "test1.txt", "4");
        assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1994", "test2.txt", "7");

        last_read_time_1_ = std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1993"]["file.0.last_read_time"].GetString());
        last_read_time_2_ = std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1994"]["file.0.last_read_time"].GetString());
        break;
      }
      case FlowState::CLEAR_SENT:
        break;
      case FlowState::SECOND_DESCRIBE_SENT: {
        auto clearedStateFound = [this, &root]() {
          return root.HasMember("corecomponentstate") &&
            root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1993") &&
            root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1994") &&
            std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1994"]["file.0.last_read_time"].GetString()) == last_read_time_2_ &&
            std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1993"]["file.0.last_read_time"].GetString()) != last_read_time_1_;
        };
        component_cleared_successfully_ = clearedStateFound();
        break;
      }
      default:
        throw std::runtime_error("Invalid flow state state when handling acknowledge message!");
    }
  }

 private:
  enum class FlowState {
    STARTED,
    FIRST_DESCRIBE_SENT,
    CLEAR_SENT,
    SECOND_DESCRIBE_SENT
  };

  std::atomic<FlowState> flow_state_{FlowState::STARTED};
  std::atomic_bool& component_cleared_successfully_;
  std::string last_read_time_1_;
  std::string last_read_time_2_;
};

int main(int argc, char **argv) {
  std::atomic_bool component_cleared_successfully{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  VerifyC2ClearCoreComponentState harness(component_cleared_successfully);
  harness.setKeyDir(args.key_dir);
  ClearCoreComponentStateHandler handler(component_cleared_successfully);
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
