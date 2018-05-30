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
#include "../TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "core/ConfigurableComponent.h"
#include "../integration/IntegrationBase.h"
#include "GetEnvironmentalSensors.h"

class PcapTestHarness : public IntegrationBase {
 public:
  PcapTestHarness() {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setTrace<minifi::processors::GetEnvironmentalSensors>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setDebug<minifi::ThreadedSchedulingAgent>();
  }

  void cleanup() {
    LogTestController::getInstance().reset();
  }

  void runAssertions() {
    assert(LogTestController::getInstance().contains("Initializing EnvironmentalSensors") == true);
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("pcap");
    assert(proc != nullptr);

    auto inv = std::dynamic_pointer_cast<minifi::processors::GetEnvironmentalSensors>(proc);
    assert(inv != nullptr);

    configuration->set("nifi.c2.enable", "false");
  }

 protected:
  char *dir;
  TestController testController;
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;

  if (argc > 1) {
    test_file_location = argv[1];
  }


  PcapTestHarness harness;

  harness.setKeyDir(key_dir);

  harness.run(test_file_location);

  return 0;
}
