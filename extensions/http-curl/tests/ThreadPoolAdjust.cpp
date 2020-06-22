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
#include <string>
#include <iostream>
#include "InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "TestBase.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "HTTPIntegrationBase.h"
#include "processors/LogAttribute.h"

class HttpTestHarness : public IntegrationBase {
 public:
  HttpTestHarness() {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<core::ProcessContext>();
    LogTestController::getInstance().setTrace<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<processors::ListenHTTP>();
    LogTestController::getInstance().setDebug<processors::ListenHTTP::WriteCallback>();
    LogTestController::getInstance().setDebug<processors::ListenHTTP::Handler>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<minifi::ThreadedSchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::TimerDrivenSchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
    configuration->set("nifi.flow.engine.threads", "1");
  }

  void cleanup() override {
    unlink(ss.str().c_str());
  }

  void runAssertions() override {
    assert(LogTestController::getInstance().contains("curl performed"));
    assert(LogTestController::getInstance().contains("Size:1024 Offset:0"));
    assert(!LogTestController::getInstance().contains("Size:0 Offset:0"));
  }

 protected:
  std::string dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);
  HttpTestHarness harness;
  harness.setKeyDir(args.key_dir);
  harness.run(args.test_file);
  return 0;
}
