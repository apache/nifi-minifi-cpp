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
#include <iostream>
#include "integration/IntegrationBase.h"
#include "unit/StatefulProcessor.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "core/state/ProcessorController.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

using minifi::processors::StatefulProcessor;
using minifi::state::ProcessorController;
using LogChecker = std::function<bool()>;

struct HookCollection {
  StatefulProcessor::HookType on_schedule_hook_;
  std::vector<StatefulProcessor::HookType> on_trigger_hooks_;
  LogChecker log_checker_;
};

class StatefulIntegrationTest : public IntegrationBase {
 public:
  StatefulIntegrationTest(const std::filesystem::path& test_file_path, std::string testCase, HookCollection hookCollection)
    : IntegrationBase(test_file_path),
      on_schedule_hook_(std::move(hookCollection.on_schedule_hook_)),
      on_trigger_hooks_(std::move(hookCollection.on_trigger_hooks_)),
      log_checker_(hookCollection.log_checker_),
      test_case_(std::move(testCase)) {
  }

  void testSetup() override {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<StatefulIntegrationTest>();
    logger_->log_info("Running test case \"{}\"", test_case_);
  }

  void updateProperties(minifi::FlowController& fc) override {
    /* This tests depends on a configuration that contains only one StatefulProcessor named statefulProcessor
     * (See TestStateTransactionality.yml)
     * In this case there are two components in the flowcontroller: first is the processor that the test uses,
     * second is the controller itself.
     * Added here some assertions to make it clear. In case any of these fail without changing the corresponding yml file,
     * that most probably means a breaking change. */
    size_t controllerVecIdx = 0;

    fc.executeOnAllComponents([this, &controllerVecIdx](minifi::state::StateController& component){
      if (controllerVecIdx == 1) {
        REQUIRE(component.getComponentName() == "FlowController");
      } else if (controllerVecIdx == 0) {
        REQUIRE(component.getComponentName() == "statefulProcessor");
        // set hooks
        const auto processController = dynamic_cast<ProcessorController*>(&component);
        REQUIRE(processController != nullptr);
        stateful_processor_ = &processController->getProcessor();
        REQUIRE(stateful_processor_);
        stateful_processor_.get().setHooks(on_schedule_hook_, on_trigger_hooks_);
      }

      ++controllerVecIdx;
    });

    // check controller vector size
    REQUIRE(controllerVecIdx == 2);
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
      return stateful_processor_.get().hasFinishedHooks() && log_checker_();
    }));
  }

 private:
  const StatefulProcessor::HookType on_schedule_hook_;
  const std::vector<StatefulProcessor::HookType> on_trigger_hooks_;
  const LogChecker log_checker_;
  const std::string test_case_;
  TypedProcessorWrapper<StatefulProcessor> stateful_processor_ = nullptr;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<StatefulIntegrationTest>::getLogger()};
};

const std::unordered_map<std::string, std::string> exampleState{{"key1", "value1"}, {"key2", "value2"}};
const std::unordered_map<std::string, std::string> exampleState2{{"key3", "value3"}, {"key4", "value4"}};

auto standardLogChecker = [] {
  const std::string logs = LogTestController::getInstance().getLogs();
  const auto errorResult = minifi::utils::string::countOccurrences(logs, "[error]");
  const auto warningResult = minifi::utils::string::countOccurrences(logs, "[warning]");
  return errorResult.second == 0 && warningResult.second == 0;
};

auto exceptionRollbackWarnings = [] {
  const std::string logs = LogTestController::getInstance().getLogs();
  const auto errorResult = minifi::utils::string::countOccurrences(logs, "[error]");
  const auto exceptionWarningResult = minifi::utils::string::countOccurrences(logs, "[warning] Caught \"Triggering rollback\"");
  const auto rollbackWarningResult = minifi::utils::string::countOccurrences(logs, "[warning] ProcessSession rollback for statefulProcessor executed");
  return errorResult.second == 0 && exceptionWarningResult.second == 1 && rollbackWarningResult.second == 1;
};

const std::unordered_map<std::string, HookCollection> testCasesToHookLists {
  {"State_is_recorded_after_committing", {
    {},
    {
      [] (core::StateManager& stateManager) {
        REQUIRE(stateManager.set(exampleState));
      },
      [] (core::StateManager& stateManager) {
        std::unordered_map<std::string, std::string> state;
        REQUIRE(stateManager.get(state));
        REQUIRE(state == exampleState);
      }
    },
    standardLogChecker
  }},
  {"State_is_discarded_after_rolling_back", {
    {},
    {
      [](core::StateManager& stateManager) {
        REQUIRE(stateManager.set(exampleState));
      },
      [](core::StateManager& stateManager) {
        REQUIRE(stateManager.set(exampleState2));
        throw std::runtime_error("Triggering rollback");
      },
      [](core::StateManager& stateManager) {
        std::unordered_map<std::string, std::string> state;
        REQUIRE(stateManager.get(state));
        REQUIRE(state == exampleState);
      }
    },
    exceptionRollbackWarnings
  }},
  {
    "Get_in_onSchedule_without_previous_state", {
    [](core::StateManager& stateManager) {
      std::unordered_map<std::string, std::string> state;
      REQUIRE(!stateManager.get(state));
      REQUIRE(state.empty());
    },
    {},
    standardLogChecker
    }
  },
  {
    "Set_in_onSchedule", {
      [](core::StateManager& stateManager) {
        REQUIRE(stateManager.set(exampleState));
      },
      {
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
        }
      },
      standardLogChecker
    }
  },
  {
    "Clear_in_onSchedule", {
      [](core::StateManager& stateManager) {
        REQUIRE(!stateManager.clear());
        REQUIRE(stateManager.set(exampleState));
        REQUIRE(stateManager.clear());
      },
      {
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(!stateManager.get(state));
          REQUIRE(state.empty());
        }
      },
      standardLogChecker
    },
  },
  {
    "Persist_in_onSchedule", {
      {
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.persist());
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
          REQUIRE(!stateManager.beginTransaction());
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
          REQUIRE(stateManager.set(exampleState));
          REQUIRE(stateManager.commit());
        }
      },
      standardLogChecker
    },
  },
  {
    "Manual_rollback", {
      {},
      {
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.rollback());
        }
      },
      standardLogChecker
    },
  },
  {
    "Get_without_previous_state", {
      {},
      {
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(!stateManager.get(state));
          REQUIRE(state.empty());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
          REQUIRE(stateManager.set(exampleState));
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
          REQUIRE(stateManager.clear());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
          REQUIRE(stateManager.persist());
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
          REQUIRE(stateManager.set(exampleState));
          std::unordered_map<std::string, std::string> state;
          REQUIRE(!stateManager.get(state));
          REQUIRE(state.empty());
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
          REQUIRE(stateManager.set(exampleState));
          REQUIRE(stateManager.set(exampleState));
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
          REQUIRE(stateManager.set(exampleState));
          REQUIRE(!stateManager.clear());
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
          REQUIRE(stateManager.set(exampleState));
          REQUIRE(stateManager.persist());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
          std::unordered_map<std::string, std::string> state;
          REQUIRE(!stateManager.get(state));
          REQUIRE(state.empty());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(!stateManager.get(state));
          REQUIRE(state.empty());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
          REQUIRE(stateManager.set(exampleState));
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(!stateManager.clear());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.persist());
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
          REQUIRE(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
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
          REQUIRE(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
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
          REQUIRE(stateManager.persist());
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.persist());
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
          REQUIRE(!stateManager.clear());
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
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
          REQUIRE(stateManager.set(exampleState));
        },
        [](core::StateManager& stateManager) {
          REQUIRE(stateManager.clear());
          throw std::runtime_error("Triggering rollback");
        },
        [](core::StateManager& stateManager) {
          std::unordered_map<std::string, std::string> state;
          REQUIRE(stateManager.get(state));
          REQUIRE(state == exampleState);
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
          REQUIRE(stateManager.persist());
          throw std::runtime_error("Triggering rollback");
        },
      },
      exceptionRollbackWarnings
    }
  }
};

TEST_CASE("Test state transactionality", "[statemanagement]") {
  for (const auto& test : testCasesToHookLists) {
    StatefulIntegrationTest statefulIntegrationTest(std::filesystem::path(TEST_RESOURCES) / "TestStateTransactionality.yml", test.first, test.second);
    statefulIntegrationTest.run();
  }
}

}  // namespace org::apache::nifi::minifi::test
