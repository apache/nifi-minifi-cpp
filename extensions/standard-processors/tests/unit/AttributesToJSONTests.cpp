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
#include <string>
#include <vector>

#include "TestBase.h"
#include "utils/TestUtils.h"
#include "AttributesToJSON.h"
#include "GetFile.h"
#include "PutFile.h"
#include "UpdateAttribute.h"
#include "LogAttribute.h"

namespace {

class AttributesToJSONTestFixture {
 public:
  const std::string TEST_FILE_CONTENT = "test_content";
  const std::string TEST_FILE_NAME = "tstFile.ext";

  AttributesToJSONTestFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::processors::AttributesToJSON>();
    LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
    LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
    LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

    dir_ = minifi::utils::createTempDir(&test_controller_);

    plan_ = test_controller_.createPlan();
    getfile_ = plan_->addProcessor("GetFile", "GetFile");
    update_attribute_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
    attribute_to_json_ = plan_->addProcessor("AttributesToJSON", "AttributesToJSON", core::Relationship("success", "description"), true);
    logattribute_ = plan_->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
    putfile_ = plan_->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);

    plan_->setProperty(getfile_, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir_);
    plan_->setProperty(putfile_, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir_);

    update_attribute_->setDynamicProperty("my_attribute", "my_value");
    update_attribute_->setDynamicProperty("other_attribute", "other_value");
    update_attribute_->setDynamicProperty("empty_attribute", "");

    std::fstream file;
    std::stringstream ss;
    ss << dir_ << utils::file::FileUtils::get_separator() << TEST_FILE_NAME;
    file.open(ss.str(), std::ios::out);
    file << TEST_FILE_CONTENT;
    file.close();
  }

  std::vector<std::string> getOutputFileContents() {
    std::vector<std::string> file_contents;

    auto callback = [&file_contents](const std::string& path, const std::string& filename) -> bool {
      std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
      std::string file_content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
      file_contents.push_back(file_content);
      return true;
    };

    utils::file::FileUtils::list_dir(dir_, callback, plan_->getLogger(), false);

    return file_contents;
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  std::string dir_;
  std::shared_ptr<core::Processor> getfile_;
  std::shared_ptr<core::Processor> update_attribute_;
  std::shared_ptr<core::Processor> attribute_to_json_;
  std::shared_ptr<core::Processor> logattribute_;
  std::shared_ptr<core::Processor> putfile_;
};

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move all attributes to a flowfile attribute", "[AttributesToJSONTests]") {
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"absolute.path\":\""+ dir_ + "/" + TEST_FILE_NAME + "\",\"empty_attribute\":\"\",\"filename\":\"" + TEST_FILE_NAME + "\",\"flow.id\":\"test\",\"my_attribute\":\"my_value\",\"other_attribute\":\"other_value\",\"path\":\"" + dir_ + "/\"}"));  // NOLINT
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move selected attributes to a flowfile attribute", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList.getName(), "my_attribute,non_existent_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"my_attribute\":\"my_value\",\"non_existent_attribute\":\"\"}"));
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Non-existent or empty selected attributes shall be written as null in JSON", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList.getName(), "my_attribute,non_existent_attribute,empty_attribute");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::NullValue.getName(), "true");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"my_attribute\":\"my_value\",\"non_existent_attribute\":null,\"empty_attribute\":null}"));
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "All non-existent or empty attributes shall be written as null in JSON", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::NullValue.getName(), "true");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"absolute.path\":\""+ dir_ + "/" + TEST_FILE_NAME + "\",\"empty_attribute\":null,\"filename\":\"" + TEST_FILE_NAME + "\",\"flow.id\":\"test\",\"my_attribute\":\"my_value\",\"other_attribute\":\"other_value\",\"path\":\"" + dir_ + "/\"}"));  // NOLINT
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "JSON attributes are written in flowfile", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::Destination.getName(), "flowfile-content");
  test_controller_.runSession(plan_);
  std::string expected_content = "{\"absolute.path\":\""+ dir_ + "/" + TEST_FILE_NAME + "\",\"empty_attribute\":\"\",\"filename\":\"" + TEST_FILE_NAME + "\",\"flow.id\":\"test\",\"my_attribute\":\"my_value\",\"other_attribute\":\"other_value\",\"path\":\"" + dir_ + "/\"}";  // NOLINT

  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == expected_content.size());
  REQUIRE(file_contents[0] == expected_content);
  REQUIRE(!LogTestController::getInstance().contains("key:JSONAttributes", std::chrono::seconds(0), std::chrono::milliseconds(0)));
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Do not include core attributes in JSON", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::IncludeCoreAttributes.getName(), "false");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"empty_attribute\":\"\",\"my_attribute\":\"my_value\",\"other_attribute\":\"other_value\"}"));
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Regex selected attributes are written in JSONAttributes attribute", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesRegularExpression.getName(), "[a-z]+y_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == TEST_FILE_CONTENT.size());
  REQUIRE(LogTestController::getInstance().contains("key:JSONAttributes value:{\"empty_attribute\":\"\",\"my_attribute\":\"my_value\"}"));
}

}  // namespace
