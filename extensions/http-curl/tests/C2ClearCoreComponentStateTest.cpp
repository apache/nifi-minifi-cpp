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
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::TailFile>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(40s, [&] { return component_cleared_successfully_.load(); }, 1s));
  }

  [[nodiscard]] std::string getFile1Location() const {
    return test_file_1_;
  }

 protected:
  void updateProperties(minifi::FlowController& flow_controller) override {
    auto setFileName = [] (const std::string& fileName, minifi::state::StateController& component){
      auto& processor = dynamic_cast<minifi::state::ProcessorController&>(component).getProcessor();
      processor.setProperty(minifi::processors::TailFile::FileName, fileName);
    };

    flow_controller.executeOnComponent("TailFile1",
      [&](minifi::state::StateController& component) {setFileName(test_file_1_, component);});
    flow_controller.executeOnComponent("TailFile2",
      [&](minifi::state::StateController& component) {setFileName(test_file_2_, component);});
  }

  TestController testController;
  std::string test_file_1_;
  std::string test_file_2_;
  const std::atomic_bool& component_cleared_successfully_;
};

class ClearCoreComponentStateHandler: public HeartbeatHandler {
 public:
  explicit ClearCoreComponentStateHandler(std::atomic_bool& component_cleared_successfully,
                                          std::shared_ptr<minifi::Configure> configuration,
                                          std::string file1Location)
    : HeartbeatHandler(std::move(configuration)),
      component_cleared_successfully_(component_cleared_successfully),
      file_1_location_(std::move(file1Location)) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    switch (flow_state_) {
      case FlowState::STARTED:
        assert(verifyLogLinePresenceInPollTime(10s, "ProcessSession committed for TailFile1"));
        assert(verifyLogLinePresenceInPollTime(10s, "ProcessSession committed for TailFile2"));
        sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889345", conn);
        flow_state_ = FlowState::FIRST_DESCRIBE_SENT;
        break;
      case FlowState::FIRST_DESCRIBE_ACK:
      case FlowState::CLEAR_SENT: {
        sendHeartbeatResponse("CLEAR", "corecomponentstate", "889346", conn, { {"corecomponent1", "TailFile1"} });
        flow_state_ = FlowState::CLEAR_SENT;
        break;
      }
      case FlowState::CLEAR_SENT_ACK:
      case FlowState::SECOND_DESCRIBE_SENT: {
        sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889347", conn);
        flow_state_ = FlowState::SECOND_DESCRIBE_SENT;
        break;
      }
      default: {}
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
        assert(!last_read_time_1_.empty());
        assert(!last_read_time_2_.empty());
        flow_state_ = FlowState::FIRST_DESCRIBE_ACK;
        break;
      }
      case FlowState::CLEAR_SENT: {
        auto tail_file_ran_again_checker = [this] {
          const auto log_contents = LogTestController::getInstance().log_output.str();
          const std::string tailing_file_pattern = "[debug] Tailing file " + file_1_location_;
          const std::string tail_file_committed_pattern = "[trace] ProcessSession committed for TailFile1";
          const std::vector<std::string> patterns = {tailing_file_pattern, tailing_file_pattern, tail_file_committed_pattern};
          return utils::StringUtils::matchesSequence(log_contents, patterns);
        };
        if (tail_file_ran_again_checker()) {
          flow_state_ = FlowState::CLEAR_SENT_ACK;
        }
        break;
      }
      case FlowState::SECOND_DESCRIBE_SENT: {
        if (!root.HasMember("corecomponentstate") ||
            !root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1993") ||
            !root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1994")) {
          break;
        }

        auto file2_state_time = std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1994"]["file.0.last_read_time"].GetString());
        auto file1_state_time = std::string(root["corecomponentstate"]["2438e3c8-015a-1000-79ca-83af40ec1993"]["file.0.last_read_time"].GetString());
        const bool clearedStateFound =
            root.HasMember("corecomponentstate") &&
            root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1993") &&
            root["corecomponentstate"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1994") &&
            file2_state_time == last_read_time_2_ &&
            file1_state_time != last_read_time_1_;

        if (clearedStateFound) {
          component_cleared_successfully_ = clearedStateFound;
        }
        break;
      }
      default: {}
    }
  }

 private:
  enum class FlowState {
    STARTED,
    FIRST_DESCRIBE_SENT,
    FIRST_DESCRIBE_ACK,
    CLEAR_SENT,
    CLEAR_SENT_ACK,
    SECOND_DESCRIBE_SENT
  };

  std::atomic<FlowState> flow_state_{FlowState::STARTED};
  std::atomic_bool& component_cleared_successfully_;
  std::string last_read_time_1_;
  std::string last_read_time_2_;
  std::string file_1_location_;
};

int main(int argc, char **argv) {
  std::atomic_bool component_cleared_successfully{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  VerifyC2ClearCoreComponentState harness(component_cleared_successfully);
  harness.setKeyDir(args.key_dir);
  ClearCoreComponentStateHandler handler(component_cleared_successfully, harness.getConfiguration(), harness.getFile1Location());
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
