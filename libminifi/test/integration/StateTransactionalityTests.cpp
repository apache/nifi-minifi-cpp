/**
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

#include <iostream>
#include "IntegrationBase.h"
#include "../StatefulProcessor.h"
#include "../TestBase.h"
#include "utils/IntegrationTestUtils.h"
#include "core/state/ProcessorController.h"

using org::apache::nifi::minifi::processors::StatefulProcessor;
using org::apache::nifi::minifi::state::ProcessorController;

namespace {
using LogChecker = std::function<bool()>;

struct HookCollection {
  StatefulProcessor::HookType onScheduleHook_;
  StatefulProcessor::HookListType onTriggerHooks_;
  LogChecker logChecker_;
};

class StatefulIntegrationTest : public IntegrationBase {
 public:
  explicit StatefulIntegrationTest(std::string testCase, HookCollection hookCollection)
    : onScheduleHook_(std::move(hookCollection.onScheduleHook_))
    , onTriggerHooks_(std::move(hookCollection.onTriggerHooks_))
    , logChecker_(hookCollection.logChecker_)
    , testCase_(std::move(testCase)) {
  }

  void testSetup() override {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<StatefulIntegrationTest>();
    logger_->log_info("Running test case \"%s\"", testCase_);
  }

  void updateProperties(std::shared_ptr<minifi::FlowController> fc) override {
    const auto controllerVec = fc->getAllComponents();
    /* This tests depends on a configuration that contains only one StatefulProcessor named statefulProcessor
     * (See TestStateTransactionality.yml)
     * In this case there are two components in the flowcontroller: first is the controller itself,
     * second is the processor that the test uses.
     * Added here some assertions to make it clear. In case any of these fail without changing the corresponding yml file,
     * that most probably means a breaking change. */
    assert(controllerVec.size() == 2);
    assert(controllerVec[0]->getComponentName() == "FlowController");
    assert(controllerVec[1]->getComponentName() == "statefulProcessor");

    // set hooks
    const auto processController = std::dynamic_pointer_cast<ProcessorController>(controllerVec[1]);
    assert(processController != nullptr);
    statefulProcessor_ = std::dynamic_pointer_cast<StatefulProcessor>(processController->getProcessor());
    assert(statefulProcessor_ != nullptr);
    statefulProcessor_->setHooks(onScheduleHook_, onTriggerHooks_);
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
      return statefulProcessor_->hasFinishedHooks() && logChecker_();
    }));
  }

 private:
  const StatefulProcessor::HookType onScheduleHook_;
  const StatefulProcessor::HookListType onTriggerHooks_;
  const LogChecker logChecker_;
  const std::string testCase_;
  std::shared_ptr<StatefulProcessor> statefulProcessor_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<StatefulIntegrationTest>::getLogger()};
};

const std::unordered_map<std::string, std::string> exampleState{{"key1", "value1"}, {"key2", "value2"}};
const std::unordered_map<std::string, std::string> exampleState2{{"key3", "value3"}, {"key4", "value4"}};

auto standardLogChecker = [] {
  const std::string logs = LogTestController::getInstance().log_output.str();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto warningResult = utils::StringUtils::countOccurrences(logs, "[warning]");
  return errorResult.second == 0 && warningResult.second == 0;
};

auto commitAndRollbackWarnings = [] {
  const std::string logs = LogTestController::getInstance().log_output.str();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto commitWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] Caught \"Process Session Operation: State manager commit failed.\"");
  const auto rollbackWarningResult = utils::StringUtils::countOccurrences(logs,
    "[warning] Caught Exception during process session rollback: Process Session Operation: State manager rollback failed.");
  return errorResult.second == 0 && commitWarningResult.second == 1 && rollbackWarningResult.second == 1;
};

auto exceptionRollbackWarnings = [] {
  const std::string logs = LogTestController::getInstance().log_output.str();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto exceptionWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] Caught \"Triggering rollback\"");
  const auto rollbackWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] ProcessSession rollback for statefulProcessor executed");
  return errorResult.second == 0 && exceptionWarningResult.second == 1 && rollbackWarningResult.second == 1;
};

const std::unordered_map<std::string, HookCollection> testCasesToHookLists {
  {"State_is_recorded_after_committing", {
    {},
    {
     [] (core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [] (core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     }
  },
  standardLogChecker
  }},
  {"State_is_discarded_after_rolling_back", {
     {},
     {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState2));
       throw std::runtime_error("Triggering rollback");
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     }
  },
  exceptionRollbackWarnings
  }},
  {
    "Get_in_onSchedule_without_previous_state", {
    [](core::CoreComponentStateManager& stateManager) {
      std::unordered_map<std::string, std::string> state;
      assert(!stateManager.get(state));
      assert(state.empty());
    },
    {},
    standardLogChecker
    }
  },
  {
    "Set_in_onSchedule", {
    [](core::CoreComponentStateManager& stateManager) {
      assert(!stateManager.set(exampleState));
      assert(stateManager.beginTransaction());
      assert(stateManager.set(exampleState));
      assert(stateManager.commit());
    },
    {
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     }
    },
    standardLogChecker
    }
  },
  {
    "Clear_in_onSchedule", {
    [](core::CoreComponentStateManager& stateManager) {
      assert(!stateManager.clear());
      assert(stateManager.beginTransaction());
      assert(stateManager.set(exampleState));
      assert(stateManager.commit());
      assert(stateManager.beginTransaction());
      assert(stateManager.clear());
      assert(stateManager.commit());
    },
    {
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(!stateManager.get(state));
      assert(state.empty());
     }
    },
    standardLogChecker
    },
  },
  {
    "Persist_in_onSchedule", {
    {
      [](core::CoreComponentStateManager& stateManager) {
        assert(stateManager.persist());
      }
      },
    {},
    standardLogChecker
    }
  },
  {
    "Manual_beginTransaction", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(!stateManager.beginTransaction());
     }
    },
    standardLogChecker
    },
  },
  {
    "Manual_commit", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
       assert(stateManager.commit());
     }
    },
    commitAndRollbackWarnings
    },
  },
  {
    "Manual_rollback", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.rollback());
     }
    },
    commitAndRollbackWarnings
    },
  },
  {
    "Get_without_previous_state", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(!stateManager.get(state));
       assert(state.empty());
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(get,get)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
       assert(stateManager.get(state));
       assert(state == exampleState);
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(get,set)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
       assert(stateManager.set(exampleState));
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(get,clear)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
       assert(stateManager.clear());
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(get,persist)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
       assert(stateManager.persist());
     }
    },
    standardLogChecker
    },
  },
  {
    "(set,!get)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
       std::unordered_map<std::string, std::string> state;
       assert(!stateManager.get(state));
       assert(state.empty());
     },
    },
    standardLogChecker
    },
  },
  {
    "(set,set)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
       assert(stateManager.set(exampleState));
     },
    },
    standardLogChecker
    },
  },
  {
    "(set,!clear)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
       assert(!stateManager.clear());
     },
    },
    standardLogChecker
    },
  },
  {
    "(set,persist)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
       assert(stateManager.persist());
     },
    },
    standardLogChecker
    },
  },
  {
    "(set),(clear,!get)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
       std::unordered_map<std::string, std::string> state;
       assert(!stateManager.get(state));
       assert(state.empty());
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(clear),(!get),(set),(get)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(!stateManager.get(state));
       assert(state.empty());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     }
    },
    standardLogChecker
    },
  },
  {
    "(set),(clear,set)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
       assert(stateManager.set(exampleState));
     },
    },
    standardLogChecker
    },
  },
  {
    "(set),(clear),(!clear)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(!stateManager.clear());
     },
    },
    standardLogChecker
    },
  },
  {
    "(set),(clear),(persist)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
     },
    },
    standardLogChecker
    },
  },
  {
    "(persist),(set),(get)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     },
    },
    standardLogChecker
    },
  },
  {
    "(persist),(set),(clear)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
     },
    },
    standardLogChecker
    },
  },
  {
    "(persist),(persist)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
     },
    },
    standardLogChecker
    },
  },
  {
    "No_change_2_rounds", {
    {},
    {
     [](core::CoreComponentStateManager&) {
     },
     [](core::CoreComponentStateManager&) {
     },
    },
    standardLogChecker
    },
  },
  {
    "(!clear)", {
    {},
    {
     [](core::CoreComponentStateManager& stateManager) {
       assert(!stateManager.clear());
     },
    },
    standardLogChecker
    },
  },
  {"(set),(get,throw)", {
     {},
     {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
       throw std::runtime_error("Triggering rollback");
     },
  },
  exceptionRollbackWarnings
  }},
  {"(set),(clear,throw),(get)", {
     {},
     {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.set(exampleState));
     },
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.clear());
       throw std::runtime_error("Triggering rollback");
     },
     [](core::CoreComponentStateManager& stateManager) {
       std::unordered_map<std::string, std::string> state;
       assert(stateManager.get(state));
       assert(state == exampleState);
     },
  },
  exceptionRollbackWarnings
  }},
  {"(set),(clear,throw),(get)", {
     {},
     {
     [](core::CoreComponentStateManager& stateManager) {
       assert(stateManager.persist());
       throw std::runtime_error("Triggering rollback");
     },
  },
  exceptionRollbackWarnings
  }}
};
}  // namespace


int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "A test file (*.yml) argument is mandatory, a second argument for test case name is optional\n";
    return EXIT_FAILURE;
  }
  const std::string testFile = argv[1];

  if (argc == 2) {
    // run all tests
    for (const auto& test : testCasesToHookLists) {
      StatefulIntegrationTest statefulIntegrationTest(test.first, test.second);
      statefulIntegrationTest.run(testFile);
    }
  } else if (argc == 3) {
    // run specified test case
    const std::string testCase = argv[2];
    auto iter = testCasesToHookLists.find(testCase);
    if (iter == testCasesToHookLists.end()) {
      std::cerr << "Test case \"" << testCase << "\" cannot be found\n";
      return EXIT_FAILURE;
    }
    StatefulIntegrationTest statefulIntegrationTest(iter->first, iter->second);
    statefulIntegrationTest.run(testFile);
  } else {
    std::cerr << "Too many arguments\n";
    return EXIT_FAILURE;
  }
}

