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

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
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

    dir_ = test_controller_.createTempDirectory();

    plan_ = test_controller_.createPlan();
    getfile_ = plan_->addProcessor("GetFile", "GetFile");
    update_attribute_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
    attribute_to_json_ = plan_->addProcessor("AttributesToJSON", "AttributesToJSON", core::Relationship("success", "description"), true);
    logattribute_ = plan_->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
    putfile_ = plan_->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);

    plan_->setProperty(getfile_, org::apache::nifi::minifi::processors::GetFile::Directory, dir_.string());
    plan_->setProperty(putfile_, org::apache::nifi::minifi::processors::PutFile::Directory, dir_.string());

    REQUIRE(update_attribute_->setDynamicProperty("my_attribute", "my_value"));
    REQUIRE(update_attribute_->setDynamicProperty("my_attribute_1", "my_value_1"));
    REQUIRE(update_attribute_->setDynamicProperty("other_attribute", "other_value"));
    REQUIRE(update_attribute_->setDynamicProperty("empty_attribute", ""));

    minifi::test::utils::putFileToDir(dir_, TEST_FILE_NAME, TEST_FILE_CONTENT);
  }

  static void assertJSONAttributesFromLog(const std::unordered_map<std::string, std::optional<std::string>>& expected_attributes) {
    auto match = LogTestController::getInstance().matchesRegex("key:JSONAttributes value:(.*)");
    REQUIRE(match);
    assertAttributes(expected_attributes, (*match)[1]);
  }

  void assertJSONAttributesFromFile(const std::unordered_map<std::string, std::optional<std::string>>& expected_attributes) {
    auto file_contents = getOutputFileContents();
    CHECK(file_contents.size() == 1);
    assertAttributes(expected_attributes, file_contents[0]);
  }

  static void assertAttributes(const std::unordered_map<std::string, std::optional<std::string>>& expected_attributes, const std::string& output_json) {
    rapidjson::Document root;
    rapidjson::ParseResult ok = root.Parse(output_json.c_str());
    REQUIRE(ok);
    CHECK(root.MemberCount() == expected_attributes.size());
    for (const auto& [key, value] : expected_attributes) {
      if (value == std::nullopt) {
        CHECK(root[key.c_str()].IsNull());
      } else {
        CHECK(std::string(root[key.c_str()].GetString()) == value.value());
      }
    }
  }

  std::vector<std::string> getOutputFileContents() {
    std::vector<std::string> file_contents;

    auto callback = [&file_contents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      std::ifstream is(path / filename, std::ifstream::binary);
      std::string file_content(std::istreambuf_iterator<char>{is}, std::istreambuf_iterator<char>{});
      file_contents.push_back(file_content);
      return true;
    };

    utils::file::FileUtils::list_dir(dir_, callback, plan_->getLogger(), false);

    return file_contents;
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  std::filesystem::path dir_;
  core::Processor* getfile_ = nullptr;
  core::Processor* update_attribute_ = nullptr;
  core::Processor* attribute_to_json_ = nullptr;
  core::Processor* logattribute_ = nullptr;
  core::Processor* putfile_ = nullptr;
};

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move all attributes to a flowfile attribute", "[AttributesToJSONTests]") {
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"absolute.path", (dir_ / "").string()},
    {"empty_attribute", ""},
    {"filename", TEST_FILE_NAME},
    {"flow.id", "test"},
    {"my_attribute", "my_value"},
    {"my_attribute_1", "my_value_1"},
    {"other_attribute", "other_value"},
    {"path", (std::filesystem::path(".") / "").string()}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move selected attributes to a flowfile attribute", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList, "my_attribute,non_existent_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"my_attribute", "my_value"},
    {"non_existent_attribute", ""}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move selected attributes with special characters to a flowfile attribute", "[AttributesToJSONTests]") {
  REQUIRE(update_attribute_->setDynamicProperty("special_attribute", "\\\""));
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList, "special_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"special_attribute", "\\\""}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Non-existent selected attributes shall be written as null in JSON", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList, "my_attribute,non_existent_attribute,empty_attribute");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::NullValue, "true");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"my_attribute", "my_value"},
    {"non_existent_attribute", std::nullopt},
    {"empty_attribute", ""}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "JSON attributes are written in flowfile", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::Destination, "flowfile-content");
  test_controller_.runSession(plan_);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"absolute.path", (dir_ / "").string()},
    {"empty_attribute", ""},
    {"filename", TEST_FILE_NAME},
    {"flow.id", "test"},
    {"my_attribute", "my_value"},
    {"my_attribute_1", "my_value_1"},
    {"other_attribute", "other_value"},
    {"path", (std::filesystem::path(".") / "").string()}
  };
  assertJSONAttributesFromFile(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Do not include core attributes in JSON", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::IncludeCoreAttributes, "false");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"empty_attribute", ""},
    {"my_attribute", "my_value"},
    {"my_attribute_1", "my_value_1"},
    {"other_attribute", "other_value"}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Regex selected attributes are written in JSONAttributes attribute", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesRegularExpression, "[a-z]+y_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"empty_attribute", ""},
    {"my_attribute", "my_value"}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Invalid destination is set", "[AttributesToJSONTests]") {
  CHECK_FALSE(plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::Destination, "invalid-destination"));
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Attributes from attributes list and regex selected attributes combined are written in JSONAttributes attribute", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesRegularExpression, "[a-z]+y_attribute");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList, "filename, path,my_attribute");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"empty_attribute", ""},
    {"filename", TEST_FILE_NAME},
    {"my_attribute", "my_value"},
    {"path", (std::filesystem::path(".") / "").string()}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Core attributes are written if they match regex", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesRegularExpression, "file.*");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::IncludeCoreAttributes, "false");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"filename", TEST_FILE_NAME}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Core attributes are written if they match attributes list", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesList, "filename, path,my_attribute");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::IncludeCoreAttributes, "false");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes {
    {"filename", TEST_FILE_NAME},
    {"path", (std::filesystem::path(".") / "").string()},
    {"my_attribute", "my_value"}
  };
  assertJSONAttributesFromLog(expected_attributes);
}

TEST_CASE_METHOD(AttributesToJSONTestFixture, "No matching attribute in list nor by regex", "[AttributesToJSONTests]") {
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::AttributesRegularExpression, "non-exist.*");
  plan_->setProperty(attribute_to_json_, org::apache::nifi::minifi::processors::AttributesToJSON::IncludeCoreAttributes, "false");
  test_controller_.runSession(plan_);
  auto file_contents = getOutputFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == TEST_FILE_CONTENT);

  const std::unordered_map<std::string, std::optional<std::string>> expected_attributes;
  assertJSONAttributesFromLog(expected_attributes);
}

}  // namespace
