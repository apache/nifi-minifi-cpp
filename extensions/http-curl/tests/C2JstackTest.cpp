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
#include <string>
#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"

class VerifyC2DescribeJstack : public VerifyC2Describe {
 public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("SchedulingAgent"));
  }
};

class DescribeJstackHandler : public HeartbeatHandler {
 public:
  virtual void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) {
    sendHeartbeatResponse("DESCRIBE", "jstack", "889398", conn);
  }

  virtual void handleAcknowledge(const rapidjson::Document& root) {
    assert(root.HasMember("Flowcontroller threadpool #0"));
  }
};

int main(int argc, char **argv) {
  cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  VerifyC2DescribeJstack harness;
  harness.setKeyDir(args.key_dir);
  DescribeJstackHandler responder;
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);
  return 0;
}
