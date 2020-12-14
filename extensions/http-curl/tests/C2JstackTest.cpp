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
#include <atomic>
#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"

class VerifyC2DescribeJstack : public VerifyC2Describe {
 public:
  explicit VerifyC2DescribeJstack(const std::atomic_bool& acknowledgement_checked) : VerifyC2Describe(), acknowledgement_checked_(acknowledgement_checked) {}
  void runAssertions() override {
    // This check was previously only confirming the presence of log sinks.
    // See: https://issues.apache.org/jira/browse/MINIFICPP-1421
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] { return acknowledgement_checked_.load(); }));
  }
 protected:
  const std::atomic_bool& acknowledgement_checked_;
};

class DescribeJstackHandler : public HeartbeatHandler {
 public:
  explicit DescribeJstackHandler(std::atomic_bool& acknowledgement_checked) : HeartbeatHandler(), acknowledgement_checked_(acknowledgement_checked) {}
  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    sendHeartbeatResponse("DESCRIBE", "jstack", "889398", conn);
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    assert(root.HasMember("Flowcontroller threadpool #0"));
    acknowledgement_checked_.store(true);
  }
 protected:
  std::atomic_bool& acknowledgement_checked_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  std::atomic_bool acknowledgement_checked{ false };
  VerifyC2DescribeJstack harness{ acknowledgement_checked };
  harness.setKeyDir(args.key_dir);
  DescribeJstackHandler responder{ acknowledgement_checked };
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);
  return 0;
}
