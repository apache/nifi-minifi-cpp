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
#include "core/state/UpdateController.h"
#include "core/state/ProcessorController.h"
#include "../TestBase.h"
#include "../KamikazeProcessor.h"
#include "utils/StringUtils.h"

/*Verify behavior in case exceptions are thrown in onSchedule or onTrigger functions
 * KamikazeProcessor is a test processor to trigger errors in these functions */
class KamikazeErrorHandlingTests : public IntegrationBase {
 public:
  void runAssertions() override {
    std::string logs = LogTestController::getInstance().log_output.str();

    auto result = utils::StringUtils::countOccurrences(logs, minifi::processors::KamikazeProcessor::OnScheduleExceptionStr);
    size_t last_pos = result.first;
    int occurrences = result.second;

    assert(occurrences > 1);  // Verify retry of onSchedule and onUnSchedule calls

    std::vector<std::string> must_appear_byorder_msgs = {minifi::processors::KamikazeProcessor::OnUnScheduleLogStr,
                                                 minifi::processors::KamikazeProcessor::OnScheduleLogStr,
                                                 minifi::processors::KamikazeProcessor::OnTriggerExceptionStr,
                                                 "[warning] ProcessSession rollback for kamikaze executed"};

    for (const auto &msg : must_appear_byorder_msgs) {
      last_pos = logs.find(msg, last_pos);
      assert(last_pos != std::string::npos);
    }

    assert(logs.find(minifi::processors::KamikazeProcessor::OnTriggerLogStr) == std::string::npos);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::KamikazeProcessor>();
  }

  void waitToVerifyProcessor() override {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    flowController_->updatePropertyValue("kamikaze", minifi::processors::KamikazeProcessor::ThrowInOnSchedule.getName(), "false");
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  void cleanup() override {}
};

/*Verify that event driven processors without incoming connections are not scheduled*/
class EventDriverScheduleErrorHandlingTests: public IntegrationBase {
 public:
  void updateProperties(std::shared_ptr<minifi::FlowController> fc) override {
    auto controller_vec = fc->getAllComponents();
    /* This tests depends on a configuration that contains only one KamikazeProcessor named kamikaze
     * (See testOnScheduleRetry.yml)
     * In this case there are two components in the flowcontroller: first is the controller itself,
     * second is the processor that the test uses.
     * Added here some assertions to make it clear. In case any of these fail without changing the corresponding yml file,
     * that most probably means a breaking change. */
    assert(controller_vec.size() == 2);
    assert(controller_vec[0]->getComponentName() == "FlowController");
    assert(controller_vec[1]->getComponentName() == "kamikaze");

    auto process_controller = dynamic_cast<org::apache::nifi::minifi::state::ProcessorController*>(controller_vec[1].get());
    assert(process_controller != nullptr);

    process_controller->getProcessor()->setSchedulingStrategy(org::apache::nifi::minifi::core::SchedulingStrategy::EVENT_DRIVEN);
  }

  void runAssertions() override {
    std::string logs = LogTestController::getInstance().log_output.str();
    assert(logs.find("EventDrivenSchedulingAgent cannot schedule processor without incoming connection!") != std::string::npos);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
  }

  void cleanup() override {}
};

int main(int argc, char **argv) {
  std::string test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
  }

  KamikazeErrorHandlingTests harness_kamikaze;

  harness_kamikaze.run(test_file_location);

  EventDriverScheduleErrorHandlingTests harness_eventdrivenerror;
  harness_eventdrivenerror.run(test_file_location);

  return 0;
}
