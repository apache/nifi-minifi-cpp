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

#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "processors/LogAttribute.h"
#include "integration/IntegrationBase.h"
#include "ProcessContextExpr.h"
#include "TestBase.h"

class TestHarness : public IntegrationBase {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::FlowController>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::ProcessContextExpr>();
    LogTestController::getInstance().setInfo<processors::LogAttribute>();
  }

  void cleanup() override {}

  void runAssertions() override {
    assert(LogTestController::getInstance().contains("key:route_check_attr value:good"));
    assert(LogTestController::getInstance().contains("key:variable_attribute value:replacement_value"));
    assert(LogTestController::getInstance().contains("ProcessSession rollback", std::chrono::seconds(1)) == false);  // No rollback happened
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    // inject the variable into the context.
    configuration->set("nifi.variable.test", "replacement_value");
  }
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
  }

  TestHarness harness;
  harness.run(test_file_location);

  return 0;
}
