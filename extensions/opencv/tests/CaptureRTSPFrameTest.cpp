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

#include "CaptureRTSPFrame.h"

#include <map>
#include <memory>
#include <fstream>
#include <utility>
#include <string>
#include <set>
#include <iostream>

#include "FlowFile.h"
#include "core/Core.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "FlowController.h"
#include "core/Processor.h"
#include "processors/LogAttribute.h"
#include "unit/ProvenanceTestHelper.h"

// TODO(_): valid capture test needs to be fixed
TEST_CASE("CaptureRTSPFrame::ValidCapture", "[!mayfail]") {
    TestController testController;

    LogTestController::getInstance().setTrace<minifi::processors::CaptureRTSPFrame>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();

    std::shared_ptr<TestPlan> plan = testController.createPlan();
    auto captureRTSP = plan->addProcessor("CaptureRTSPFrame", "CaptureRTSPFrame");
    // the RTSP url below comes from a public RTSP stream (hopefully still alive by the time you read this)
    // alternatively, we can set our own server using vlc.
    // vlc -vvv --loop <input video> --sout '#rtp{port=1234,sdp=rtsp://127.0.0.1:port/test}' --sout-keep
    // then the uri will be rtsp://127.0.0.1:port/test
    plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPHostname, "170.93.143.139");
    plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPURI, "rtplive/470011e600ef003a004ee33696235daa");
    plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPPort, "");
    plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::ImageEncoding, ".jpg");

    testController.runSession(plan, true);
    std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
    REQUIRE(record);
    REQUIRE(LogTestController::getInstance().contains("A frame is captured"));
}

TEST_CASE("CaptureRTSPFrame::InvalidURI", "[opencvtest2]") {
  TestController testController;

  LogTestController::getInstance().setTrace<minifi::processors::CaptureRTSPFrame>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto captureRTSP = plan->addProcessor("CaptureRTSPFrame", "CaptureRTSPFrame");

  plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPHostname, "170.93.143.139");
  plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPURI, "abcd");
  plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::RTSPPort, "");
  plan->setProperty(captureRTSP, minifi::processors::CaptureRTSPFrame::ImageEncoding, ".jpg");

  plan->addProcessor(
          "LogAttribute",
          "Log",
          core::Relationship("failure", "description"),
          true);

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Unable to open RTSP stream"));
}
