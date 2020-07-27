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
#include <cstdio>
#include <memory>
#include <string>
#include <iostream>

#include "core/logging/Logger.h"
#include "FlowController.h"
#include "TestBase.h"
#include "processors/TailFile.h"
#include "processors/LogAttribute.h"
#include "state/ProcessorController.h"
#include "integration/IntegrationBase.h"
#include "utils/IntegrationTestUtils.h"

class TailFileTestHarness : public IntegrationBase {
 public:
  TailFileTestHarness() : IntegrationBase(1000) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);

    statefile = dir + utils::file::FileUtils::get_separator();
    statefile += "statefile";
    std::fstream file;
    ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "Lin\\e1\nli\\nen\nli\\ne3\nli\\ne4\nli\\ne5\n";
    file.close();
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
    LogTestController::getInstance().setTrace<minifi::FlowController>();
  }

  void cleanup() override {
    std::remove(ss.str().c_str());
    std::remove(statefile.c_str());
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "5 flowfiles were received from TailFile input",
        "Looking for delimiter 0xA",
        "li\\ne5"));
  }

 protected:
  void updateProperties(std::shared_ptr<minifi::FlowController> fc) override {
    for (auto &comp : fc->getComponents("tf")) {
      std::shared_ptr<minifi::state::ProcessorController> proc = std::dynamic_pointer_cast<minifi::state::ProcessorController>(comp);
      if (nullptr != proc) {
        proc->getProcessor()->setProperty(minifi::processors::TailFile::FileName, ss.str());
        proc->getProcessor()->setProperty(minifi::processors::TailFile::StateFile, statefile);
      }
    }
  }

  std::string statefile;
  std::string dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  std::string test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
  }

  TailFileTestHarness harness;

  harness.run(test_file_location);

  return 0;
}
