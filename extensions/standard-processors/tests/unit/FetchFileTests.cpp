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

#include "TestBase.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "processors/GenerateFlowFile.h"
#include "processors/FetchFile.h"
#include "processors/PutFile.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

namespace {

class FetchFileTestFixture {
 public:
  FetchFileTestFixture();
  ~FetchFileTestFixture();
  std::vector<std::string> getSuccessfulFlowFileContents() const;
  std::vector<std::string> getFailedFlowFileContents() const;
  std::vector<std::string> getNotFoundFlowFileContents() const;
#ifndef WIN32
  std::vector<std::string> getPermissionDeniedFlowFileContents() const;
#endif

 protected:
  std::vector<std::string> getDirContents(const std::string& dir_path) const;

  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  const std::string input_dir_;
  const std::string success_output_dir_;
  const std::string failure_output_dir_;
  const std::string not_found_output_dir_;
  const std::string permission_denied_output_dir_;
  const std::string permission_denied_file_name_;
  const std::string input_file_name_;
  const std::string file_content_;
  std::shared_ptr<core::Processor> fetch_file_processor_;
  std::shared_ptr<core::Processor> update_attribute_processor_;
};

FetchFileTestFixture::FetchFileTestFixture()
  : plan_(test_controller_.createPlan()),
    input_dir_(test_controller_.createTempDirectory()),
    success_output_dir_(test_controller_.createTempDirectory()),
    failure_output_dir_(test_controller_.createTempDirectory()),
    not_found_output_dir_(test_controller_.createTempDirectory()),
    permission_denied_output_dir_(test_controller_.createTempDirectory()),
    permission_denied_file_name_("permission_denied.txt"),
    input_file_name_("test.txt"),
    file_content_("The quick brown fox jumps over the lazy dog\n")  {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::FetchFile>();
  LogTestController::getInstance().setTrace<minifi::processors::GenerateFlowFile>();

  REQUIRE(!input_dir_.empty());
  REQUIRE(!success_output_dir_.empty());
  REQUIRE(!failure_output_dir_.empty());
  REQUIRE(!not_found_output_dir_.empty());
  REQUIRE(!permission_denied_output_dir_.empty());

  auto generate_flow_file_processor = plan_->addProcessor("GenerateFlowFile", "GenerateFlowFile");
  plan_->setProperty(generate_flow_file_processor, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "0B");
  update_attribute_processor_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
  plan_->setProperty(update_attribute_processor_, "absolute.path", input_dir_, true);
  plan_->setProperty(update_attribute_processor_, "filename", input_file_name_, true);

  fetch_file_processor_ = plan_->addProcessor("FetchFile", "FetchFile", core::Relationship("success", "description"), true);

  auto success_putfile = plan_->addProcessor("PutFile", "SuccessPutFile", { {"success", "d"} }, false);
  plan_->addConnection(fetch_file_processor_, {"success", "d"}, success_putfile);
  success_putfile->setAutoTerminatedRelationships({{"success", "d"}, {"failure", "d"}});
  plan_->setProperty(success_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), success_output_dir_);

  auto failure_putfile = plan_->addProcessor("PutFile", "FailurePutFile", { {"success", "d"} }, false);
  plan_->addConnection(fetch_file_processor_, {"failure", "d"}, failure_putfile);
  failure_putfile->setAutoTerminatedRelationships({{"success", "d"}, {"failure", "d"}});
  plan_->setProperty(failure_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), failure_output_dir_);

  auto not_found_putfile = plan_->addProcessor("PutFile", "NotFoundPutFile", { {"success", "d"} }, false);
  plan_->addConnection(fetch_file_processor_, {"not.found", "d"}, not_found_putfile);
  not_found_putfile->setAutoTerminatedRelationships({{"success", "d"}, {"not.found", "d"}});
  plan_->setProperty(not_found_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), not_found_output_dir_);

  auto permission_denied_putfile = plan_->addProcessor("PutFile", "PermissionDeniedPutFile", { {"success", "d"} }, false);
  plan_->addConnection(fetch_file_processor_, {"permission.denied", "d"}, permission_denied_putfile);
  not_found_putfile->setAutoTerminatedRelationships({{"success", "d"}, {"permission.denied", "d"}});
  plan_->setProperty(permission_denied_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), permission_denied_output_dir_);

  utils::putFileToDir(input_dir_, input_file_name_, file_content_);
  utils::putFileToDir(input_dir_, permission_denied_file_name_, file_content_);
#ifndef WIN32
  utils::file::FileUtils::set_permissions(input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_, 0);
#endif
}

FetchFileTestFixture::~FetchFileTestFixture() {
#ifndef WIN32
  utils::file::FileUtils::set_permissions(input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_, 0644);
#endif
}

std::vector<std::string> FetchFileTestFixture::getDirContents(const std::string& dir_path) const {
  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
    file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::FileUtils::list_dir(dir_path, lambda, plan_->getLogger(), false);
  return file_contents;
}

std::vector<std::string> FetchFileTestFixture::getSuccessfulFlowFileContents() const {
  return getDirContents(success_output_dir_);
}

std::vector<std::string> FetchFileTestFixture::getFailedFlowFileContents() const {
  return getDirContents(failure_output_dir_);
}

std::vector<std::string> FetchFileTestFixture::getNotFoundFlowFileContents() const {
  return getDirContents(not_found_output_dir_);
}

#ifndef WIN32
std::vector<std::string> FetchFileTestFixture::getPermissionDeniedFlowFileContents() const {
  return getDirContents(permission_denied_output_dir_);
}
#endif

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file with default but non-existent file path", "[testFetchFile]") {
  plan_->setProperty(update_attribute_processor_, "filename", "non_existent.file", true);
  test_controller_.runSession(plan_);
  auto file_contents = getNotFoundFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].empty());
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[error] File to fetch was not found"));
}

TEST_CASE_METHOD(FetchFileTestFixture, "FileToFetch property set to a non-existent file path", "[testFetchFile]") {
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::FileToFetch.getName(), "/tmp/non_existent.file");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::LogLevelWhenFileNotFound.getName(), "INFO");
  test_controller_.runSession(plan_);
  auto file_contents = getNotFoundFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].empty());
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[info] File to fetch was not found"));
}

#ifndef WIN32
TEST_CASE_METHOD(FetchFileTestFixture, "Permission denied to read file", "[testFetchFile]") {
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::FileToFetch.getName(),
    input_dir_ + utils::file::FileUtils::get_separator() + permission_denied_file_name_);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::LogLevelWhenPermissionDenied.getName(), "WARN");
  test_controller_.runSession(plan_);
  auto file_contents = getPermissionDeniedFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].empty());
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "[warning] Read permission denied for file"));
}
#endif

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file with default file path", "[testFetchFile]") {
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
}

TEST_CASE_METHOD(FetchFileTestFixture, "Test fetching file from a custom path", "[testFetchFile]") {
  REQUIRE(0 == utils::file::FileUtils::create_dir(input_dir_ + utils::file::FileUtils::get_separator() + "sub"));
  utils::putFileToDir(input_dir_ + utils::file::FileUtils::get_separator() + "sub", input_file_name_, file_content_);
  auto file_path = input_dir_ + utils::file::FileUtils::get_separator() + "sub" + utils::file::FileUtils::get_separator() + input_file_name_;
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::FileToFetch.getName(), file_path);
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(utils::file::FileUtils::exists(file_path));
}

TEST_CASE_METHOD(FetchFileTestFixture, "Flow scheduling fails due to missing move destination directory when completion strategy is set to move file", "[testFetchFile]") {
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_), minifi::Exception);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Flow fails due to move conflict", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy.getName(), "Fail");
  test_controller_.runSession(plan_);
  auto file_contents = getFailedFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].empty());

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == "old content");
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move specific properties are ignored when completion strategy is not move file", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy.getName(), "Fail");
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with replace file", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy.getName(), "Replace File");
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with renaming file to a new random filename", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy.getName(), "Rename");
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));


  auto move_dir_contents = getDirContents(move_dir);
  REQUIRE(move_dir_contents.size() == 2);
  REQUIRE(move_dir_contents[0] != move_dir_contents[1]);
  for (const auto& content : move_dir_contents) {
    REQUIRE((content == file_content_ || content == "old content"));
  }
}

TEST_CASE_METHOD(FetchFileTestFixture, "Move destination conflict is resolved with deleting the new file and keeping the old one", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::putFileToDir(move_dir, input_file_name_, "old content");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveConflictStrategy.getName(), "Keep Existing");
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == "old content");
}

TEST_CASE_METHOD(FetchFileTestFixture, "Fetched file is moved to a new directory after flow completion", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

TEST_CASE_METHOD(FetchFileTestFixture, "After flow completion the fetched file is moved to a non-existent directory which is created by the flow", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  move_dir = move_dir + utils::file::FileUtils::get_separator() + "temp";
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));

  std::ifstream is(move_dir + utils::file::FileUtils::get_separator() + input_file_name_, std::ifstream::binary);
  REQUIRE(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()) == file_content_);
}

#ifndef WIN32
TEST_CASE_METHOD(FetchFileTestFixture, "Move completion strategy failure due to filesystem error still succeeds flow", "[testFetchFile]") {
  auto move_dir = test_controller_.createTempDirectory();
  utils::file::FileUtils::set_permissions(move_dir, 0);
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Move File");
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::MoveDestinationDirectory.getName(), move_dir);
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "completion strategy failed"));
  utils::file::FileUtils::set_permissions(move_dir, 0644);
}
#endif

TEST_CASE_METHOD(FetchFileTestFixture, "Fetched file is deleted after flow completion", "[testFetchFile]") {
  plan_->setProperty(fetch_file_processor_, org::apache::nifi::minifi::processors::FetchFile::CompletionStrategy.getName(), "Delete File");
  test_controller_.runSession(plan_);
  auto file_contents = getSuccessfulFlowFileContents();
  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0] == file_content_);
  REQUIRE(!utils::file::FileUtils::exists(input_dir_ + utils::file::FileUtils::get_separator() + input_file_name_));
}

}  // namespace
