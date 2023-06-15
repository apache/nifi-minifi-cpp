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
#include <cassert>
#include "IntegrationBase.h"
#include "core/logging/Logger.h"
#include "core/Scheduling.h"
#include "core/state/ProcessorController.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "../../../extensions/test-processors/KamikazeProcessor.h"
#include "utils/StringUtils.h"
#include "utils/IntegrationTestUtils.h"

/*Verify behavior in case exceptions are thrown in onSchedule or onTrigger functions
 * KamikazeProcessor is a test processor to trigger errors in these functions */
class KamikazeErrorHandlingTests : public IntegrationBase {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(wait_time_, [&] {
      const std::string logs = LogTestController::getInstance().getLogs();
      const auto result = utils::StringUtils::countOccurrences(logs, minifi::processors::KamikazeProcessor::OnScheduleExceptionStr);
      const int occurrences = result.second;
      return 1 < occurrences;
    }));
    flowController_->updatePropertyValue("kamikaze", std::string(minifi::processors::KamikazeProcessor::ThrowInOnSchedule.name), "false");

    const std::vector<std::string> must_appear_byorder_msgs = {minifi::processors::KamikazeProcessor::OnUnScheduleLogStr,
                                                 minifi::processors::KamikazeProcessor::OnScheduleLogStr,
                                                 minifi::processors::KamikazeProcessor::OnTriggerExceptionStr,
                                                 "[warning] ProcessSession rollback for kamikaze executed"};

    const bool test_success = verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
      const std::string logs = LogTestController::getInstance().getLogs();
      const auto result = utils::StringUtils::countOccurrences(logs, minifi::processors::KamikazeProcessor::OnScheduleExceptionStr);
      size_t last_pos = result.first;
      for (const std::string& msg : must_appear_byorder_msgs) {
        last_pos = logs.find(msg, last_pos);
        if (last_pos == std::string::npos)  {
          return false;
        }
      }
      return true;
    });
    assert(test_success);

    assert(LogTestController::getInstance().getLogs().find(minifi::processors::KamikazeProcessor::OnTriggerLogStr) == std::string::npos);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::KamikazeProcessor>();
  }
};

/*Verify that event driven processors without incoming connections are not scheduled*/
class EventDriverScheduleErrorHandlingTests: public IntegrationBase {
 public:
  void updateProperties(minifi::FlowController& fc) override {
    /* This tests depends on a configuration that contains only one KamikazeProcessor named kamikaze
     * (See testOnScheduleRetry.yml)
     * In this case there are two components in the flowcontroller: first is the processor that the test uses,
     * second is the controller itself.
     * Added here some assertions to make it clear. In case any of these fail without changing the corresponding yml file,
     * that most probably means a breaking change. */
    size_t controllerVecIdx = 0;

    fc.executeOnAllComponents([&controllerVecIdx](org::apache::nifi::minifi::state::StateController& component){
      if (controllerVecIdx == 1) {
        assert(component.getComponentName() == "FlowController");
      } else if (controllerVecIdx == 0) {
        assert(component.getComponentName() == "kamikaze");

        auto process_controller = dynamic_cast<org::apache::nifi::minifi::state::ProcessorController*>(&component);
        assert(process_controller != nullptr);

        process_controller->getProcessor().setSchedulingStrategy(org::apache::nifi::minifi::core::SchedulingStrategy::EVENT_DRIVEN);
      }

      ++controllerVecIdx;
    });

    // check controller vector size
    assert(controllerVecIdx == 2);
  }

  void runAssertions() override {
    std::string logs = LogTestController::getInstance().getLogs();
    assert(logs.find("EventDrivenSchedulingAgent cannot schedule processor without incoming connection!") != std::string::npos);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
  }
};

int main(int argc, char **argv) {
  std::string test_file_location;
  std::string url;
  if (argc > 1) {
    test_file_location = argv[1];
  }

  KamikazeErrorHandlingTests harness_kamikaze;

  harness_kamikaze.run(test_file_location);

  EventDriverScheduleErrorHandlingTests harness_eventdrivenerror;
  harness_eventdrivenerror.run(test_file_location);

  return 0;
}
