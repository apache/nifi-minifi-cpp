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

#include <filesystem>
#include <fstream>

#include "LmdbContentRepository.h"
#include "ResourceClaim.h"
#include "properties/Configure.h"
#include "unit/Catch.h"
#include "unit/ContentRepositoryDependentTests.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::test {

class LmdbContentRepositoryTests : TestController {
 public:
  LmdbContentRepositoryTests() {
    auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
    configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, createTempDirectory().string());
    REQUIRE(content_repo_->initialize(configuration));
  }

 protected:
  static constexpr std::string_view test_content_ = "well hello there";
  std::shared_ptr<extensions::lmdb::LmdbContentRepository> content_repo_ = std::make_shared<extensions::lmdb::LmdbContentRepository>();

  void writeContent(const minifi::ResourceClaim& claim) {
    auto stream = content_repo_->write(claim);
    stream->write(as_bytes(std::span(test_content_)));
    stream->close();
  }
};

TEST_CASE("Invalid or empty dbsize configuration value is set", "[lmdb]") {
  TestController controller;
  LogTestController::getInstance().setDebug<extensions::lmdb::LmdbWrapper>();
  auto db_path = controller.createTempDirectory().string();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, db_path);
  SECTION("Invalid value") {
    configuration->set(minifi::Configure::nifi_content_repository_lmdb_max_db_size, "invalid");
    REQUIRE_THROWS_WITH(content_repo->initialize(configuration),
        "nifi.content.repository.lmdb.max.db.size was set to invalid value: 'invalid', but got GeneralParsingError (Parsing Error:0)");
  }
  SECTION("Empty value") {
    configuration->set(minifi::Configure::nifi_content_repository_lmdb_max_db_size, "");
    REQUIRE(content_repo->initialize(configuration));
    REQUIRE(LogTestController::getInstance().contains("Setting LMDB max DB size to 10737418240 bytes"));
  }
}

TEST_CASE("Valid dbsize configuration value is set", "[lmdb]") {
  TestController controller;
  LogTestController::getInstance().setDebug<extensions::lmdb::LmdbWrapper>();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, controller.createTempDirectory().string());
  configuration->set(minifi::Configure::nifi_content_repository_lmdb_max_db_size, "100 MB");
  REQUIRE(content_repo->initialize(configuration));
  REQUIRE(LogTestController::getInstance().contains("Setting LMDB max DB size to 104857600 bytes"));
}

TEST_CASE("Initialize succeeds when target directory already exists", "[lmdb]") {
  TestController controller;
  auto db_path = controller.createTempDirectory();
  REQUIRE(std::filesystem::exists(db_path));
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, db_path.string());
  REQUIRE(content_repo->initialize(configuration));
}

TEST_CASE("Initialize fails and is safe to destroy when the directory path is a regular file", "[lmdb]") {
  TestController controller;
  const auto file_path = controller.createTempDirectory() / "not_a_directory";
  {
    std::ofstream file(file_path);
    file << "this is a regular file, not a directory";
  }
  REQUIRE(std::filesystem::is_regular_file(file_path));

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, file_path.string());

  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE_FALSE(content_repo->initialize(configuration));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Key does not exist in empty database", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Written value exists", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim);
  REQUIRE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "exists returns false for unrelated claim when other content was written", "[lmdb]") {
  auto written_claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*written_claim);
  auto unrelated_claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  REQUIRE(content_repo_->exists(*written_claim));
  REQUIRE_FALSE(content_repo_->exists(*unrelated_claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Empty content claim does not exist", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto write_stream = content_repo_->write(*claim);
  write_stream->close();
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Multiple claims coexist independently", "[lmdb]") {
  auto claim1 = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto claim2 = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto claim3 = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim1);
  writeContent(*claim2);
  writeContent(*claim3);
  REQUIRE(content_repo_->exists(*claim1));
  REQUIRE(content_repo_->exists(*claim2));
  REQUIRE(content_repo_->exists(*claim3));
  REQUIRE(content_repo_->getRepositoryEntryCount() == 3);
  REQUIRE(content_repo_->remove(*claim2));
  REQUIRE(content_repo_->exists(*claim1));
  REQUIRE_FALSE(content_repo_->exists(*claim2));
  REQUIRE(content_repo_->exists(*claim3));
  REQUIRE(content_repo_->getRepositoryEntryCount() == 2);
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Reading nonexistent claim returns STREAM_ERROR", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto read_stream = content_repo_->read(*claim);
  std::vector<std::byte> buffer(8);
  REQUIRE(minifi::io::isError(read_stream->read(as_writable_bytes(std::span(buffer)))));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Read written value", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim);
  auto read_stream = content_repo_->read(*claim);
  std::vector<std::byte> buffer(test_content_.size());
  auto bytes_read = read_stream->read(as_writable_bytes(std::span(buffer)));
  read_stream->close();
  REQUIRE(bytes_read == test_content_.size());
  REQUIRE(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size()) == test_content_);
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Removing a nonexistent claim succeeds", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  REQUIRE_FALSE(content_repo_->exists(*claim));
  REQUIRE(content_repo_->remove(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Removing an existing value", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim);
  REQUIRE(content_repo_->exists(*claim));
  REQUIRE(content_repo_->remove(*claim));
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "clearOrphans is a no-op on an empty repository", "[lmdb]") {
  REQUIRE(content_repo_->getRepositoryEntryCount() == 0);
  content_repo_->clearOrphans();
  REQUIRE(content_repo_->getRepositoryEntryCount() == 0);
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Clear orphan values", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim);
  REQUIRE(content_repo_->exists(*claim));
  content_repo_->reset();
  content_repo_->clearOrphans();
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Empty repository reports zero size and entry count", "[lmdb]") {
  REQUIRE(content_repo_->getRepositoryEntryCount() == 0);
  REQUIRE(content_repo_->getRepositorySize() == 0);
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Written value updates repository stats", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  writeContent(*claim);
  REQUIRE(content_repo_->getRepositoryEntryCount() == 1);
  REQUIRE(content_repo_->getRepositorySize() > 0);
}

TEST_CASE("Content persists across LmdbContentRepository re-initialization", "[lmdb]") {
  TestController controller;
  auto db_path = controller.createTempDirectory();
  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, db_path.string());

  std::string claim_path;
  static constexpr std::string_view content = "persisted content";
  {
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(configuration));
    auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo);
    claim_path = claim->getContentFullPath();
    auto stream = content_repo->write(*claim);
    stream->write(as_bytes(std::span(content)));
    stream->close();
    // ensure the content is not deleted on resource claim destruction
    content_repo->incrementStreamCount(*claim);
  }

  auto reopened_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE(reopened_repo->initialize(configuration));
  auto reopened_claim = std::make_shared<minifi::ResourceClaimImpl>(claim_path, reopened_repo);
  REQUIRE(reopened_repo->exists(*reopened_claim));
  auto read_stream = reopened_repo->read(*reopened_claim);
  std::vector<std::byte> buffer(content.size());
  REQUIRE(read_stream->read(as_writable_bytes(std::span(buffer))) == content.size());
  REQUIRE(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size()) == content);
}

TEST_CASE("ProcessSession::read reads the flowfile from offset to size", "[lmdb]") {
  ContentRepositoryDependentTests::testReadOnSmallerClonedFlowFiles(std::make_shared<extensions::lmdb::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::append should append to the flowfile and set its size correctly", "[lmdb]") {
  ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<extensions::lmdb::LmdbContentRepository>());
  ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<extensions::lmdb::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::read can read zero length flowfiles without crash", "[lmdb]") {
  ContentRepositoryDependentTests::testReadFromZeroLengthFlowFile(std::make_shared<extensions::lmdb::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::write can be cancelled", "[lmdb]") {
  ContentRepositoryDependentTests::testOkWrite(std::make_shared<extensions::lmdb::LmdbContentRepository>());
  ContentRepositoryDependentTests::testErrWrite(std::make_shared<extensions::lmdb::LmdbContentRepository>());
  ContentRepositoryDependentTests::testCancelWrite(std::make_shared<extensions::lmdb::LmdbContentRepository>());
}

}  // namespace org::apache::nifi::minifi::test
