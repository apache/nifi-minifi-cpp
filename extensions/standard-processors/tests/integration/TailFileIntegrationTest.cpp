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
#include <cstdio>
#include <string>
#include <iostream>
#include <filesystem>

#include "FlowController.h"
#include "unit/TestBase.h"
#include "processors/TailFile.h"
#include "processors/LogAttribute.h"
#include "state/ProcessorController.h"
#include "integration/IntegrationBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "utils/TimeUtil.h"
#include "utils/TimeZoneUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class TailFileTestHarness : public IntegrationBase {
 public:
  explicit TailFileTestHarness(const std::filesystem::path& test_file_location)
      : IntegrationBase(test_file_location) {
    dir = testController.createTempDirectory();

    statefile = dir / "statefile";
    std::fstream file;
    file.open(dir / "tstFile.ext", std::ios::out);
    file << "Lin\\e1\nli\\nen\nli\\ne3\nli\\ne4\nli\\ne5\n";
    file.close();
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
    LogTestController::getInstance().setTrace<minifi::FlowController>();
#ifdef WIN32
    minifi::utils::timeutils::dateSetInstall(TZ_DATA_DIR);
#endif
  }

  void cleanup() override {
    std::filesystem::remove(dir / "tstFile.ext");
    std::filesystem::remove(statefile);
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(wait_time_,
        "5 flowfiles were received from TailFile input",
        "Looking for delimiter 0xA",
        "li\\ne5"));
  }

 protected:
  void updateProperties(minifi::FlowController& fc) override {
    fc.executeOnComponent("tf", [this] (minifi::state::StateController& component) {
      auto proc = dynamic_cast<minifi::state::ProcessorController*>(&component);
      if (nullptr != proc) {
        REQUIRE(proc->getProcessor().setProperty(minifi::processors::TailFile::FileName.name, (dir / "tstFile.ext").string()));
        REQUIRE(proc->getProcessor().setProperty(minifi::processors::TailFile::StateFile.name, statefile.string()));
      }
    });
  }

  std::filesystem::path statefile;
  std::filesystem::path dir;
  TestController testController;
};

TEST_CASE("TailFile integration test", "[tailfile]") {
  std::filesystem::path test_file_path;
  SECTION("Timer driven") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestTailFile.yml";
  }
  SECTION("Cron driven") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestTailFileCron.yml";
  }
  TailFileTestHarness harness(test_file_path);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
