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

#ifdef WIN32
#include <Windows.h>
#endif
#include <cstring>
#include <list>

#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/Literals.h"
#include "core/repository/FileSystemRepository.h"
#include "utils/IntegrationTestUtils.h"
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
  auto config = std::make_shared<minifi::Configure>();
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

  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  CHECK(verifyEventHappenedInPollTime(5s, [&] {
      const auto end_memory = minifi::utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
      REQUIRE(end_memory > 0);
      return end_memory < start_memory + int64_t{5_MB};
    }, 100ms));
}

TEST_CASE("FileSystemRepository can clear orphan entries") {
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());
  {
    auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
    REQUIRE(content_repo->initialize(configuration));

    minifi::ResourceClaim claim(content_repo);
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
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());

  auto content_repo = std::make_shared<TestFileSystemRepository>();
  REQUIRE(content_repo->initialize(configuration));
  std::string filename;
  {
    minifi::ResourceClaim claim(content_repo);
    content_repo->write(claim)->write("hi");
    auto files = minifi::utils::file::list_dir_all(dir, testController.getLogger());
    REQUIRE(files.size() == 1);
    // ensure that the content is not deleted during resource claim destruction
    filename = (files[0].first / files[0].second).string();
#ifdef WIN32
    REQUIRE(SetFileAttributes(filename.c_str(), FILE_ATTRIBUTE_READONLY));
#else
    minifi::utils::file::set_permissions(dir, 0555);
#endif
  }

#ifdef WIN32
  REQUIRE(SetFileAttributes(filename.c_str(), GetFileAttributes(filename.c_str()) & ~FILE_ATTRIBUTE_READONLY));
#else
  minifi::utils::file::set_permissions(dir, 0777);
#endif
  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 1);
  {
    minifi::ResourceClaim claim(content_repo);
    content_repo->write(claim)->write("hi");
    REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 2);
  }

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  REQUIRE(content_repo->getPurgeList().empty());
}

TEST_CASE("FileSystemRepository removes non-existing resource file from purge list") {
  TestController testController;
  auto dir = testController.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());

  auto content_repo = std::make_shared<TestFileSystemRepository>();
  REQUIRE(content_repo->initialize(configuration));
  std::string filename;
  {
    minifi::ResourceClaim claim(content_repo);
    content_repo->write(claim)->write("hi");
    auto files = minifi::utils::file::list_dir_all(dir, testController.getLogger());
    REQUIRE(files.size() == 1);
    // ensure that the content is not deleted during resource claim destruction
    filename = (files[0].first / files[0].second).string();
#ifdef WIN32
    REQUIRE(SetFileAttributes(filename.c_str(), FILE_ATTRIBUTE_READONLY));
#else
    minifi::utils::file::set_permissions(dir, 0555);
#endif
  }

#ifdef WIN32
  REQUIRE(SetFileAttributes(filename.c_str(), GetFileAttributes(filename.c_str()) & ~FILE_ATTRIBUTE_READONLY));
#else
  minifi::utils::file::set_permissions(dir, 0777);
#endif
  REQUIRE(std::filesystem::remove(filename));
  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  {
    minifi::ResourceClaim claim(content_repo);
    content_repo->write(claim)->write("hi");
    REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).size() == 1);
  }

  REQUIRE(minifi::utils::file::list_dir_all(dir, testController.getLogger()).empty());
  REQUIRE(content_repo->getPurgeList().empty());
}

}  // namespace org::apache::nifi::minifi::test
