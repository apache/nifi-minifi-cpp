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

// loading extensions increases the baseline memory usage
// as we measure the absolute memory usage that would fail this test
#define EXTENSION_LIST ""  // NOLINT(cppcoreguidelines-macro-usage)

#include <list>

#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "unit/TestUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/Literals.h"
#include "core/repository/FileSystemRepository.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class TestFileSystemRepository : public minifi::core::repository::FileSystemRepository {
 public:
  using FileSystemRepository::FileSystemRepository;
  std::list<std::string> getPurgeList() const {
    return purge_list_;
  }
};

TEST_CASE("Test Physical memory usage", "[testphysicalmemoryusage]") {
  TestController controller;
  auto dir = controller.createTempDirectory();
  auto fs_repo = std::make_shared<minifi::core::repository::FileSystemRepository>();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());
  REQUIRE(fs_repo->initialize(config));
  const auto start_memory = minifi::utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
  REQUIRE(start_memory > 0);

  auto content_session = fs_repo->createSession();
  auto resource_id = content_session->create();
  auto stream = content_session->write(resource_id);
  size_t file_size = 20_MB;
  std::span<const char> fragment = "well, hello there";
  for (size_t i = 0; i < file_size / fragment.size() + 1; ++i) {
    stream->write(as_bytes(fragment));
  }

  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  CHECK(verifyEventHappenedInPollTime(5s, [&] {
      const auto end_memory = minifi::utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
      REQUIRE(end_memory > 0);
      return end_memory < start_memory + int64_t{5_MB};
    }, 100ms));
}

TEST_CASE("FileSystemRepository can clear orphan entries") {
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());
  {
    auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
    REQUIRE(content_repo->initialize(configuration));

    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    // ensure that the content is not deleted during resource claim destruction
    content_repo->incrementStreamCount(claim);
  }

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 1);

  auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
  REQUIRE(content_repo->initialize(configuration));
  content_repo->clearOrphans();

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
}

TEST_CASE("FileSystemRepository can retry removing entry that previously failed to be removed") {
  if (utils::runningAsUnixRoot())
    SKIP("Cannot test insufficient permissions with root user");
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());

  auto content_repo = std::make_shared<TestFileSystemRepository>();
  REQUIRE(content_repo->initialize(configuration));
  {
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    auto files = minifi::utils::file::list_dir_all(dir, testController.getLogger());
    REQUIRE(files.size() == 1);
    // ensure that the content is not deleted during resource claim destruction
    utils::makeFileOrDirectoryNotWritable(dir);
  }

  utils::makeFileOrDirectoryWritable(dir);
  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 1);
  {
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 2);
  }

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  REQUIRE(content_repo->getPurgeList().empty());
}

TEST_CASE("FileSystemRepository removes non-existing resource file from purge list") {
  if (utils::runningAsUnixRoot())
    SKIP("Cannot test insufficient permissions with root user");
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());

  auto content_repo = std::make_shared<TestFileSystemRepository>();
  REQUIRE(content_repo->initialize(configuration));
  std::string filename;
  {
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    auto files = minifi::utils::file::list_dir_all(dir, testController.getLogger());
    REQUIRE(files.size() == 1);
    // ensure that the content is not deleted during resource claim destruction
    filename = (files[0].first / files[0].second).string();
    utils::makeFileOrDirectoryNotWritable(dir);
  }

  utils::makeFileOrDirectoryWritable(dir);
  REQUIRE(std::filesystem::remove(filename));
  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  {
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 1);
  }

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  REQUIRE(content_repo->getPurgeList().empty());
}

TEST_CASE("Append Claim") {
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto content_repo = std::make_shared<TestFileSystemRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());
  REQUIRE(content_repo->initialize(configuration));


  const std::string content = "well hello there";

  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo);
  content_repo->write(*claim)->write(as_bytes(std::span(content)));

  // requesting append before content end fails
  CHECK(content_repo->lockAppend(*claim, 0) == nullptr);
  auto lock = content_repo->lockAppend(*claim, content.length());
  // trying to append to the end succeeds
  CHECK(lock != nullptr);
  // simultaneously trying to append to the same claim fails
  CHECK(content_repo->lockAppend(*claim, content.length()) == nullptr);

  // manually deleting append lock
  lock.reset();

  // appending after lock is released succeeds
  lock = content_repo->lockAppend(*claim, content.length());
  CHECK(lock != nullptr);

  const std::string appended = "General Kenobi!";
  content_repo->write(*claim, true)->write(as_bytes(std::span(appended)));

  lock.reset();

  // size has changed
  CHECK(content_repo->lockAppend(*claim, content.length()) == nullptr);

  CHECK(content_repo->lockAppend(*claim, content.length() + appended.length()) != nullptr);
}


}  // namespace org::apache::nifi::minifi::test
