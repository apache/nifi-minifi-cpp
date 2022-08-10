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
#include "Catch.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"

class VerifyC2DescribeCoreComponentState : public VerifyC2Describe {
 public:
  explicit VerifyC2DescribeCoreComponentState(std::atomic_bool& verified)
    : VerifyC2Describe(verified) {
    temp_dir_ = testController.createTempDirectory();

    test_file_1_ = utils::file::FileUtils::concat_path(temp_dir_, "test1.txt");
    test_file_2_ = utils::file::FileUtils::concat_path(temp_dir_, "test2.txt");

    std::ofstream f1(test_file_1_, std::ios::out | std::ios::binary);
    f1 << "foo\n";

    std::ofstream f2(test_file_2_, std::ios::out | std::ios::binary);
    f2 << "foobar\n";
  }

 protected:
  void updateProperties(minifi::FlowController& flow_controller) override {
    auto setFileName = [] (const std::string& fileName, minifi::state::StateController& component){
      auto& processor = dynamic_cast<minifi::state::ProcessorController&>(component).getProcessor();
      processor.setProperty(minifi::processors::TailFile::FileName, fileName);
    };

    flow_controller.executeOnComponent("TailFile1",
      [&](minifi::state::StateController& component) {setFileName(test_file_1_, component);});
    flow_controller.executeOnComponent("TailFile2",
      [&](minifi::state::StateController& component) {setFileName(test_file_2_, component);});
  }

  TestController testController;
  std::string temp_dir_;
  std::string test_file_1_;
  std::string test_file_2_;
};

class DescribeCoreComponentStateHandler: public HeartbeatHandler {
 public:
  explicit DescribeCoreComponentStateHandler(std::shared_ptr<minifi::Configure> configuration, std::atomic_bool& verified)
    : HeartbeatHandler(std::move(configuration)),
      verified_(verified) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    sendHeartbeatResponse("DESCRIBE", "corecomponentstate", "889345", conn);
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
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

    assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1993", "test1.txt", "4");
    assertExpectedTailFileState("2438e3c8-015a-1000-79ca-83af40ec1994", "test2.txt", "7");
    verified_ = true;
  }

 private:
  std::atomic_bool& verified_;
};

int main(int argc, char **argv) {
  std::atomic_bool verified{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  VerifyC2DescribeCoreComponentState harness(verified);
  harness.setKeyDir(args.key_dir);
  DescribeCoreComponentStateHandler handler(harness.getConfiguration(), verified);
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
