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
#include "../../integration/IntegrationBase.h"
#include "../../TestBase.h"

class TestHarness : public IntegrationBase {
 public:
  TestHarness() {
    log_entry_found = false;
  }

  void testSetup() {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setInfo<processors::LogAttribute>();
  }

  void cleanup() {
  }

  void runAssertions() {
    assert(log_entry_found);
  }

  void waitToVerifyProcessor() {
    // This test takes a while to complete -> wait at most 10 secs
    log_entry_found = LogTestController::getInstance().contains("key:route_check_attr value:good", std::chrono::seconds(10));
    log_entry_found = LogTestController::getInstance().contains("key:variable_attribute value:replacement_value", std::chrono::seconds(10));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    // inject the variable into the context.
    configuration->set("nifi.variable.test", "replacement_value");
  }

 protected:
  bool log_entry_found;
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
