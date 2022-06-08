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
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include "TestBase.h"
#include "Catch.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "processors/FetchFile.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"
#include "SingleProcessorTestController.h"

using namespace std::literals::chrono_literals;

namespace {

class FetchFileTestFixture {
 public:
  FetchFileTestFixture();
  ~FetchFileTestFixture();

 protected:
  [[nodiscard]] std::unordered_multiset<std::string> getDirContents(const std::string& dir_path) const;

  std::shared_ptr<minifi::processors::FetchFile> fetch_file_processor_;
  std::shared_ptr<minifi::test::SingleProcessorTestController> test_controller_;
  const std::string input_dir_;
  const std::string permission_denied_file_name_;
  const std::string input_file_name_;
  const std::string file_content_;
  std::unordered_map<std::string, std::string> attributes_;
};

FetchFileTestFixture::FetchFileTestFixture()
  : fetch_file_processor_(std::make_shared<minifi::processors::FetchFile>("FetchFile")),
    test_controller_(std::make_shared<minifi::test::SingleProcessorTestController>(fetch_file_processor_)),
    input_dir_(test_controller_->createTempDirectory()),
    permission_denied_file_name_("permission_denied.txt"),
    input_file_name_("test.txt"),
    file_content_("The quick brown fox jumps over the lazy dog\n")  {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::FetchFile>();

  attributes_ = {{"absolute.path", input_dir_}, {"filename", input_file_name_}};

  utils::putFileToDir(input_dir_, input_file_name_, file_content_);
  utils::putFileToDir(input_dir_, permission_denied_file_name_, file_content_);
  std::filesystem::permissions(input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_, static_cast<std::filesystem::perms>(0));
}

FetchFileTestFixture::~FetchFileTestFixture() {
  std::filesystem::permissions(input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_, static_cast<std::filesystem::perms>(0644));
}

std::unordered_multiset<std::string> FetchFileTestFixture::getDirContents(const std::string& dir_path) const {
  std::unordered_multiset<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
    file_contents.insert(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::FileUtils::list_dir(dir_path, lambda, test_controller_->plan->getLogger(), false);
  return file_contents;
}

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file with default but non-existent file path", "[testFetchFile]") {
  attributes_["filename"] = "non_existent.file";
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::NotFound);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == "");
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[error] File to fetch was not found"));
}

TEST_CASE_METHOD(FetchFileTestFixture, "FileToFetch property set to a non-existent file path", "[testFetchFile]") {
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::FileToFetch, "/tmp/non_existent.file");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::LogLevelWhenFileNotFound, "INFO");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::NotFound);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == "");
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[info] File to fetch was not found"));
}

#ifndef WIN32
TEST_CASE_METHOD(FetchFileTestFixture, "Permission denied to read file", "[testFetchFile]") {
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::FileToFetch,
    input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::LogLevelWhenPermissionDenied, "WARN");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::PermissionDenied);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == "");
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[warning] Read permission denied for file"));
}
#endif

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file with default file path", "[testFetchFile]") {
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
}

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file from a custom path", "[testFetchFile]") {
  REQUIRE(0 == utils::file::FileUtils::create_dir(input_dir_ + utils::file::FileUtils::get_separator() + "sub"));
  utils::putFileToDir(input_dir_ + utils::file::FileUtils::get_separator() + "sub", input_file_name_, file_content_);
  auto file_path = input_dir_ + utils::file::FileUtils::get_separator() + "sub" + utils::file::FileUtils::get_separator() + input_file_name_;
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::FileToFetch, file_path);
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(utils::file::FileUtils::exists(file_path));
}

TEST_CASE_METHOD(FetchFileTestFixture, "Flow scheduling fails due to missing move destination directory when completion strategy is set to move file", "[testFetchFile]") {
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  REQUIRE_THROWS_AS(test_controller_->trigger("", attributes_), minifi::Exception);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Flow fails due to move conflict", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy, "Fail");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Failure);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == "");

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == "old content");
  REQUIRE(utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move specific properties are ignored when completion strategy is not move file", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy, "Fail");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with replace file", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy, "Replace File");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with renaming file to a new random filename", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy, "Rename");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  auto move_dir_contents = getDirContents(move_dir);
  std::unordered_multiset<std::string> expected = {"old content", file_content_};
  REQUIRE(move_dir_contents == expected);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with deleting the new file and keeping the old one", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy, "Keep Existing");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == "old content");
}

TEST_CASE_METHOD(FetchFileTestFixture, "Fetched file is moved to a new directory after flow completion", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "After flow completion the fetched file is moved to a non-existent directory which is created by the flow", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  move_dir = move_dir + utils::file::FileUtils::get_separator() + "temp";
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

#ifndef WIN32
TEST_CASE_METHOD(FetchFileTestFixture, "Move completion strategy failure due to filesystem error still succeeds flow", "[testFetchFile]") {
  auto move_dir = test_controller_->createTempDirectory();
  utils::file::FileUtils::set_permissions(move_dir, 0);
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Move File");
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory, move_dir);
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "completion strategy failed"));
  utils::file::FileUtils::set_permissions(move_dir, 0644);
}
#endif

TEST_CASE_METHOD(FetchFileTestFixture, "Fetched file is deleted after flow completion", "[testFetchFile]") {
  fetch_file_processor_->setProperty(org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy, "Delete File");
  const auto result = test_controller_->trigger("", attributes_);
  auto file_contents = result.at(minifi::processors::FetchFile::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller_->plan->getContent(file_contents[0]) == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
}

}  // namespace
