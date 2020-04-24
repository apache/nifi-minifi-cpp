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

class DescribeManifestHandler: public HeartbeatHandler {
public:

  explicit DescribeManifestHandler(bool isSecure)
      : HeartbeatHandler(isSecure) {
  }

  virtual void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) {
    sendHeartbeatResponse("DESCRIBE", "manifest", "889345", conn);
  }

  virtual void handleAcknowledge(const rapidjson::Document& root) {
    verifyJsonHasAgentManifest(root);
  }
};
int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  url = "http://localhost:0/api/heartbeat";
  if (argc > 1) {
    test_file_location = argv[1];
    if (argc > 2) {
      url = "https://localhost:0/api/heartbeat";
      key_dir = argv[2];
    }
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }

  VerifyC2Describe harness(isSecure);

  harness.setKeyDir(key_dir);

  DescribeManifestHandler responder(isSecure);

  harness.setUrl(url, &responder);

  harness.run(test_file_location);
}
