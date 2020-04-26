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
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"

class VerifyC2DescribeCoreComponentState : public VerifyC2Describe {
 public:
  explicit VerifyC2DescribeCoreComponentState(bool isSecure)
      : VerifyC2Describe(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    temp_dir_ = testController.createTempDirectory(format);

    test_file_1_ = utils::file::FileUtils::concat_path(temp_dir_, "test1.txt");
    test_file_2_ = utils::file::FileUtils::concat_path(temp_dir_, "test2.txt");

    std::ofstream f1(test_file_1_);
    f1 << "foo";

    std::ofstream f2(test_file_2_);
    f2 << "foobar";
  }

 protected:
  void updateProperties(std::shared_ptr<minifi::FlowController> flow_controller) override {
    std::dynamic_pointer_cast<minifi::state::ProcessorController>(flow_controller->getComponents("TailFile1")[0])
        ->getProcessor()->setProperty(minifi::processors::TailFile::FileName, test_file_1_);
    std::dynamic_pointer_cast<minifi::state::ProcessorController>(flow_controller->getComponents("TailFile2")[0])
        ->getProcessor()->setProperty(minifi::processors::TailFile::FileName, test_file_2_);
  }

  TestController testController;
  std::string temp_dir_;
  std::string test_file_1_;
  std::string test_file_2_;
};

class DescribeCoreComponentStateHandler: public HeartbeatHandler {
 public:

  explicit DescribeCoreComponentStateHandler(bool isSecure)
      : HeartbeatHandler(isSecure) {
  }

  virtual void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) {
    sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889345", conn);
  }

  virtual void handleAcknowledge(const rapidjson::Document& root) {
    assert(root.HasMember("corecomponentstate"));

    auto assertExpectedTailFileState = [&](const char* uuid, const char* name, const char* position) {
      assert(root["corecomponentstate"].HasMember(uuid));
      const auto& tf = root["corecomponentstate"][uuid];
      assert(tf.HasMember("file.0.name"));
      assert(std::string(tf["file.0.name"].GetString()) == name);
      assert(tf.HasMember("file.0.position"));
      assert(std::string(tf["file.0.position"].GetString()) == position);
      assert(tf.HasMember("file.0.current"));
      assert(strlen(tf["file.0.current"].GetString()) > 0U);
    };

    assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1993", "test1.txt", "3");
    assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1994", "test2.txt", "6");
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

  VerifyC2DescribeCoreComponentState harness(isSecure);

  harness.setKeyDir(key_dir);

  DescribeCoreComponentStateHandler responder(isSecure);

  harness.setUrl(url, &responder);

  harness.run(test_file_location);

  return 0;
}
