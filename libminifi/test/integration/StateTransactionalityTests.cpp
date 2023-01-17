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
#include "../Catch.h"
#include "utils/IntegrationTestUtils.h"
#include "core/state/ProcessorController.h"

using org::apache::nifi::minifi::processors::StatefulProcessor;
using org::apache::nifi::minifi::state::ProcessorController;

namespace {
using LogChecker = std::function<bool()>;

struct HookCollection {
  StatefulProcessor::HookType on_schedule_hook_;
  std::vector<StatefulProcessor::HookType> on_trigger_hooks_;
  LogChecker log_checker_;
};

class StatefulIntegrationTest : public IntegrationBase {
 public:
  explicit StatefulIntegrationTest(std::string testCase, HookCollection hookCollection)
    : on_schedule_hook_(std::move(hookCollection.on_schedule_hook_))
    , on_trigger_hooks_(std::move(hookCollection.on_trigger_hooks_))
    , log_checker_(hookCollection.log_checker_)
    , test_case_(std::move(testCase)) {
  }

  void testSetup() override {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<StatefulIntegrationTest>();
    logger_->log_info("Running test case \"%s\"", test_case_);
  }

  void updateProperties(minifi::FlowController& fc) override {
    /* This tests depends on a configuration that contains only one StatefulProcessor named statefulProcessor
     * (See TestStateTransactionality.yml)
     * In this case there are two components in the flowcontroller: first is the processor that the test uses,
     * second is the controller itself.
     * Added here some assertions to make it clear. In case any of these fail without changing the corresponding yml file,
     * that most probably means a breaking change. */
    size_t controllerVecIdx = 0;

    fc.executeOnAllComponents([this, &controllerVecIdx](org::apache::nifi::minifi::state::StateController& component){
      if (controllerVecIdx == 1) {
        assert(component.getComponentName() == "FlowController");
      } else if (controllerVecIdx == 0) {
        assert(component.getComponentName() == "statefulProcessor");
        // set hooks
        const auto processController = dynamic_cast<ProcessorController*>(&component);
        assert(processController != nullptr);
        stateful_processor_ = dynamic_cast<StatefulProcessor*>(&processController->getProcessor());
        assert(stateful_processor_);
        stateful_processor_->setHooks(on_schedule_hook_, on_trigger_hooks_);
      }

      ++controllerVecIdx;
    });

    // check controller vector size
    assert(controllerVecIdx == 2);
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
      return stateful_processor_->hasFinishedHooks() && log_checker_();
    }));
  }

 private:
  const StatefulProcessor::HookType on_schedule_hook_;
  const std::vector<StatefulProcessor::HookType> on_trigger_hooks_;
  const LogChecker log_checker_;
  const std::string test_case_;
  StatefulProcessor* stateful_processor_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<StatefulIntegrationTest>::getLogger()};
};

const std::unordered_map<std::string, std::string> exampleState{{"key1", "value1"}, {"key2", "value2"}};
const std::unordered_map<std::string, std::string> exampleState2{{"key3", "value3"}, {"key4", "value4"}};

auto standardLogChecker = [] {
  const std::string logs = LogTestController::getInstance().getLogs();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto warningResult = utils::StringUtils::countOccurrences(logs, "[warning]");
  return errorResult.second == 0 && warningResult.second == 0;
};

auto commitAndRollbackWarnings = [] {
  const std::string logs = LogTestController::getInstance().getLogs();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto commitWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] Caught \"Process Session Operation: State manager commit failed.\"");
  const auto rollbackWarningFirst = utils::StringUtils::countOccurrences(logs, "[warning] Caught Exception during process session rollback");
  const auto rollbackWarningSecond = utils::StringUtils::countOccurrences(logs, "Process Session Operation: State manager rollback failed.");
  return errorResult.second == 0 && commitWarningResult.second == 1 && rollbackWarningFirst.second == 1 && rollbackWarningSecond.second == 1;
};

auto exceptionRollbackWarnings = [] {
  const std::string logs = LogTestController::getInstance().getLogs();
  const auto errorResult = utils::StringUtils::countOccurrences(logs, "[error]");
  const auto exceptionWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] Caught \"Triggering rollback\"");
  const auto rollbackWarningResult = utils::StringUtils::countOccurrences(logs, "[warning] ProcessSession rollback for statefulProcessor executed");
  return errorResult.second == 0 && exceptionWarningResult.second == 1 && rollbackWarningResult.second == 1;
};

const std::unordered_map<std::string, HookCollection> testCasesToHookLists {
  {"State_is_recorded_after_committing", {
    {},
    {
      [] (core::StateManager& stateManager) {
        assert(stateManager.set(exampleState));
      },
      [] (core::StateManager& stateManager) {
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
      [](core::StateManager& stateManager) {
        assert(stateManager.set(exampleState));
      },
      [](core::StateManager& stateManager) {
        assert(stateManager.set(exampleState2));
        throw std::runtime_error("Triggering rollback");
      },
      [](core::StateManager& stateManager) {
        std::unordered_map<std::string, std::string> state;
        assert(stateManager.get(state));
        assert(state == exampleState);
      }
    },
    exceptionRollbackWarnings
  }},
  {
    "Get_in_onSchedule_without_previous_state", {
    [](core::StateManager& stateManager) {
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
      [](core::StateManager& stateManager) {
        assert(stateManager.set(exampleState));
      },
      {
        [](core::StateManager& stateManager) {
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
      [](core::StateManager& stateManager) {
        assert(!stateManager.clear());
        assert(stateManager.set(exampleState));
        assert(stateManager.clear());
      },
      {
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          assert(!stateManager.get(state));
          assert(state.empty());
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager& stateManager) {
          assert(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
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
        [](core::StateManager&) {},
        [](core::StateManager&) {},
      },
      standardLogChecker
    },
  },
  {
    "(!clear)", {
      {},
      {
        [](core::StateManager& stateManager) {
          assert(!stateManager.clear());
        },
      },
      standardLogChecker
    },
  },
  {
    "(set),(get,throw)", {
      {},
      {
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          assert(stateManager.get(state));
          assert(state == exampleState);
          throw std::runtime_error("Triggering rollback");
        },
      },
      exceptionRollbackWarnings
    }
  },
  {
    "(set),(clear,throw),(get)", {
      {},
      {
        [](core::StateManager& stateManager) {
          assert(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          assert(stateManager.clear());
          throw std::runtime_error("Triggering rollback");
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          assert(stateManager.get(state));
          assert(state == exampleState);
        },
      },
      exceptionRollbackWarnings
    }
  },
  {
    "(set),(clear,throw),(get)", {
      {},
      {
        [](core::StateManager& stateManager) {
          assert(stateManager.persist());
          throw std::runtime_error("Triggering rollback");
        },
      },
      exceptionRollbackWarnings
    }
  }
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

