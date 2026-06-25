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

#include "LmdbFlowFileRepository.h"
#include "LmdbContentRepository.h"
#include "Connection.h"
#include "FlowFileRecord.h"
#include "ResourceClaim.h"
#include "io/BufferStream.h"
#include "properties/Configure.h"
#include "utils/Id.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::test {

namespace {
std::shared_ptr<minifi::FlowFileRecord> createFlowFileWithContent(core::ContentRepository& content_repo, std::string_view content) {
  auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();
  const auto content_session = content_repo.createSession();
  const auto claim = content_session->create();
  const auto stream = content_session->write(claim);
  stream->write(std::as_bytes(std::span(content)));
  flow_file->setResourceClaim(claim);
  flow_file->setSize(stream->size());
  flow_file->setOffset(0);
  stream->close();
  content_session->commit();
  return flow_file;
}
}  // namespace

class LmdbFlowFileRepositoryTests : TestController {
 public:
  LmdbFlowFileRepositoryTests() {
    db_path_ = createTempDirectory();
    auto configuration = std::make_shared<minifi::ConfigureImpl>();
    configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path_.string());
    REQUIRE(flow_file_repo_->initialize(configuration));
    content_db_path_ = createTempDirectory();
    configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_db_path_.string());
    REQUIRE(content_repo_->initialize(configuration));
    flow_file_repo_->loadComponent(content_repo_);
  }

 protected:
  std::filesystem::path db_path_;
  std::filesystem::path content_db_path_;
  std::shared_ptr<extensions::lmdb::LmdbFlowFileRepository> flow_file_repo_ = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  std::shared_ptr<extensions::lmdb::LmdbContentRepository> content_repo_ = std::make_shared<extensions::lmdb::LmdbContentRepository>();
};

TEST_CASE("Initialize LmdbFlowFileRepository", "[lmdb]") {
  TestController controller;

  std::filesystem::path db_path;
  SECTION("initialize succeeds when the directory does not exist") {
    db_path = controller.createTempDirectory() / "does_not_exist_yet";
    REQUIRE_FALSE(std::filesystem::exists(db_path));
  }

  SECTION("initialize succeeds when the directory already exists") {
    db_path = controller.createTempDirectory();
    REQUIRE(std::filesystem::exists(db_path));
  }

  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path.string());

  auto flow_file_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(flow_file_repo->initialize(configuration));
  REQUIRE(std::filesystem::exists(db_path));
}

TEST_CASE("initialize honors a valid max db size and persists data", "[lmdb]") {
  TestController controller;
  LogTestController::getInstance().setDebug<extensions::lmdb::LmdbWrapper>();
  const auto db_path = controller.createTempDirectory();
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path.string());
  configuration->set(minifi::Configure::nifi_flowfile_repository_lmdb_max_db_size, "32 MB");

  auto flow_file_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(flow_file_repo->initialize(configuration));
  REQUIRE(LogTestController::getInstance().contains("Setting LMDB max DB size to 33554432 bytes"));
}

TEST_CASE("initialize throws on an invalid max db size", "[lmdb]") {
  TestController controller;
  const auto db_path = controller.createTempDirectory();
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path.string());
  configuration->set(minifi::Configure::nifi_flowfile_repository_lmdb_max_db_size, "not-a-size");

  auto flow_file_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE_THROWS(flow_file_repo->initialize(configuration));
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Put on empty repository increases entry count", "[lmdb]") {
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 0);
  static constexpr std::string_view payload = "hello flowfile";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 1);
  REQUIRE(flow_file_repo_->getRepositorySize() > 0);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Put with same key overwrites and keeps entry count at 1", "[lmdb]") {
  static constexpr std::string_view first = "first";
  static constexpr std::string_view second = "second value, longer than first";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(first.data()), first.size()));
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(second.data()), second.size()));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 1);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Put of multiple distinct keys", "[lmdb]") {
  static constexpr std::string_view payload = "data";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  REQUIRE(flow_file_repo_->Put("key2", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  REQUIRE(flow_file_repo_->Put("key3", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 3);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Put of empty value succeeds", "[lmdb]") {
  REQUIRE(flow_file_repo_->Put("key1", nullptr, 0));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 1);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Get on empty repository returns false", "[lmdb]") {
  std::string value = "untouched";
  REQUIRE_FALSE(flow_file_repo_->Get("missing", value));
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Put then Get round-trips the value", "[lmdb]") {
  static constexpr std::string_view payload = "hello flowfile";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  std::string value;
  REQUIRE(flow_file_repo_->Get("key1", value));
  REQUIRE(value == payload);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Get returns false for nonexistent key after other Puts", "[lmdb]") {
  static constexpr std::string_view payload = "data";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
  std::string value;
  REQUIRE_FALSE(flow_file_repo_->Get("key2", value));
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Get reflects the latest Put for a key", "[lmdb]") {
  static constexpr std::string_view first = "first";
  static constexpr std::string_view second = "second value";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(first.data()), first.size()));
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(second.data()), second.size()));
  std::string value;
  REQUIRE(flow_file_repo_->Get("key1", value));
  REQUIRE(value == second);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "MultiPut with empty vector succeeds and adds nothing", "[lmdb]") {
  REQUIRE(flow_file_repo_->MultiPut({}));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 0);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "MultiPut writes all entries", "[lmdb]") {
  std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>> data;
  data.emplace_back("key1", std::make_unique<minifi::io::BufferStream>(std::string{"value-one"}));
  data.emplace_back("key2", std::make_unique<minifi::io::BufferStream>(std::string{"value-two"}));
  data.emplace_back("key3", std::make_unique<minifi::io::BufferStream>(std::string{"value-three"}));
  REQUIRE(flow_file_repo_->MultiPut(data));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 3);
  std::string value;
  REQUIRE(flow_file_repo_->Get("key1", value));
  REQUIRE(value == "value-one");
  REQUIRE(flow_file_repo_->Get("key2", value));
  REQUIRE(value == "value-two");
  REQUIRE(flow_file_repo_->Get("key3", value));
  REQUIRE(value == "value-three");
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "MultiPut overwrites existing keys", "[lmdb]") {
  static constexpr std::string_view old_value = "old";
  REQUIRE(flow_file_repo_->Put("key1", reinterpret_cast<const uint8_t*>(old_value.data()), old_value.size()));

  std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>> data;
  data.emplace_back("key1", std::make_unique<minifi::io::BufferStream>(std::string{"new-one"}));
  data.emplace_back("key2", std::make_unique<minifi::io::BufferStream>(std::string{"new-two"}));
  REQUIRE(flow_file_repo_->MultiPut(data));

  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 2);
  std::string value;
  REQUIRE(flow_file_repo_->Get("key1", value));
  REQUIRE(value == "new-one");
  REQUIRE(flow_file_repo_->Get("key2", value));
  REQUIRE(value == "new-two");
}

TEST_CASE("MultiPut entries persist across LmdbFlowFileRepository re-open", "[lmdb]") {
  TestController controller;
  auto db_path = controller.createTempDirectory();
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path.string());

  {
    auto flow_file_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(flow_file_repo->initialize(configuration));
    std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>> data;
    data.emplace_back("key1", std::make_unique<minifi::io::BufferStream>(std::string{"persisted-one"}));
    data.emplace_back("key2", std::make_unique<minifi::io::BufferStream>(std::string{"persisted-two"}));
    REQUIRE(flow_file_repo->MultiPut(data));
  }

  auto reopened_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(reopened_repo->initialize(configuration));
  REQUIRE(reopened_repo->getRepositoryEntryCount() == 2);
  std::string value;
  REQUIRE(reopened_repo->Get("key1", value));
  REQUIRE(value == "persisted-one");
  REQUIRE(reopened_repo->Get("key2", value));
  REQUIRE(value == "persisted-two");
}

TEST_CASE("Put persists across LmdbFlowFileRepository re-open", "[lmdb]") {
  TestController controller;
  auto db_path = controller.createTempDirectory();
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_flowfile_repository_directory_default, db_path.string());

  static constexpr std::string_view payload = "persisted flowfile";
  {
    auto flow_file_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(flow_file_repo->initialize(configuration));
    REQUIRE(flow_file_repo->Put("key1", reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
    REQUIRE(flow_file_repo->getRepositoryEntryCount() == 1);
  }

  auto reopened_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(reopened_repo->initialize(configuration));
  REQUIRE(reopened_repo->getRepositoryEntryCount() == 1);
}

TEST_CASE_METHOD(LmdbFlowFileRepositoryTests, "Deleting keys is done in batches after flush", "[lmdb]") {
  std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>> data;
  data.emplace_back("key1", std::make_unique<minifi::io::BufferStream>(std::string{"value-one"}));
  data.emplace_back("key2", std::make_unique<minifi::io::BufferStream>(std::string{"value-two"}));
  data.emplace_back("key3", std::make_unique<minifi::io::BufferStream>(std::string{"value-three"}));
  REQUIRE(flow_file_repo_->MultiPut(data));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 3);
  REQUIRE(flow_file_repo_->Delete("key1"));
  REQUIRE(flow_file_repo_->Delete("key2"));
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 3);
  flow_file_repo_->flush();
  REQUIRE(flow_file_repo_->getRepositoryEntryCount() == 1);
  std::string value;
  REQUIRE_FALSE(flow_file_repo_->Get("key1", value));
  REQUIRE_FALSE(flow_file_repo_->Get("key1", value));
  REQUIRE(flow_file_repo_->Get("key3", value));
  REQUIRE(value == "value-three");
}

TEST_CASE("loadComponent restores a persisted flow file into its connection", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  const auto connection_id = minifi::utils::IdGenerator::getIdGenerator()->generate();
  minifi::utils::Identifier ff_id;

  {
    auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(config));
    auto connection = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

    auto flow_file = createFlowFileWithContent(*content_repo, "hello");
    ff_id = flow_file->getUUID();
    flow_file->setConnection(connection.get());
    REQUIRE(flow_file->Persist(ff_repo));
    ff_repo->flush();
  }

  {
    auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(config));
    auto connection = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

    ff_repo->setConnectionMap({{connection_id.to_string(), connection.get()}});
    ff_repo->loadComponent(content_repo);

    REQUIRE(connection->getQueueSize() == 1);
    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto restored = connection->poll(expired);
    REQUIRE(expired.empty());
    REQUIRE(restored);
    REQUIRE(restored->getUUID() == ff_id);
  }
}

TEST_CASE("loadComponent purges a flow file whose connection is unknown", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  {
    auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(config));
    auto connection = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection");

    auto flow_file = createFlowFileWithContent(*content_repo, "hello");
    flow_file->setConnection(connection.get());
    REQUIRE(flow_file->Persist(ff_repo));
    ff_repo->flush();
    REQUIRE(ff_repo->getRepositoryEntryCount() == 1);
  }

  {
    auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(config));

    // No connection map set, so the persisted flow file has no owning container.
    ff_repo->loadComponent(content_repo);
    REQUIRE(ff_repo->getRepositoryEntryCount() == 0);
  }
}

TEST_CASE("loadComponent purges an entry that cannot be deserialized", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));
  static constexpr std::string_view garbage = "this is not a serialized flow file";
  REQUIRE(ff_repo->Put("key1", reinterpret_cast<const uint8_t*>(garbage.data()), garbage.size()));
  REQUIRE(ff_repo->getRepositoryEntryCount() == 1);

  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE(content_repo->initialize(config));
  ff_repo->loadComponent(content_repo);

  REQUIRE(ff_repo->getRepositoryEntryCount() == 0);
}

TEST_CASE("loadComponent applies the content size health check", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  size_t expected_queue_size = 0;
  SECTION("health check enabled drops the undersized flow file") {
    config->set(minifi::Configure::nifi_flow_file_repository_check_health, "true");
    expected_queue_size = 1;  // only the healthy flow file is restored
  }
  SECTION("health check disabled keeps the undersized flow file") {
    config->set(minifi::Configure::nifi_flow_file_repository_check_health, "false");
    expected_queue_size = 2;  // both flow files are restored
  }

  const auto connection_id = minifi::utils::IdGenerator::getIdGenerator()->generate();
  auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));
  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE(content_repo->initialize(config));
  auto connection = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

  auto healthy_flow_file = createFlowFileWithContent(*content_repo, "foo");
  healthy_flow_file->setConnection(connection.get());
  REQUIRE(healthy_flow_file->Persist(ff_repo));

  auto undersized_flow_file = createFlowFileWithContent(*content_repo, "bar");
  undersized_flow_file->setConnection(connection.get());
  undersized_flow_file->setSize(undersized_flow_file->getSize() * 2);  // corrupt the flow file so it fails the health check
  REQUIRE(undersized_flow_file->Persist(ff_repo));

  ff_repo->setConnectionMap({{connection_id.to_string(), connection.get()}});
  REQUIRE(connection->getQueueSize() == 0);
  ff_repo->loadComponent(content_repo);
  REQUIRE(connection->getQueueSize() == expected_queue_size);
}

TEST_CASE("loadComponent clears orphaned content from the content repository", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  // Write content with no referencing flow file
  {
    auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
    REQUIRE(content_repo->initialize(config));
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("orphan");
    // ensure that the content is not deleted during resource claim destruction
    content_repo->incrementStreamCount(claim);
    REQUIRE(content_repo->getRepositoryEntryCount() == 1);
  }

  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE(content_repo->initialize(config));
  REQUIRE(content_repo->getRepositoryEntryCount() == 1);

  auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));

  ff_repo->loadComponent(content_repo);
  REQUIRE(content_repo->getRepositoryEntryCount() == 0);
}

TEST_CASE("flush decreases the owned count and removes content for a deleted flow file", "[lmdb]") {
  TestController controller;
  const auto ff_dir = controller.createTempDirectory();
  const auto content_dir = controller.createTempDirectory();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  auto ff_repo = std::make_shared<extensions::lmdb::LmdbFlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));
  auto content_repo = std::make_shared<extensions::lmdb::LmdbContentRepository>();
  REQUIRE(content_repo->initialize(config));
  ff_repo->loadComponent(content_repo);

  auto flow_file = createFlowFileWithContent(*content_repo, "payload");
  REQUIRE(flow_file->Persist(ff_repo));
  REQUIRE(content_repo->getRepositoryEntryCount() == 1);

  REQUIRE(ff_repo->Delete(flow_file));
  ff_repo->flush();
  REQUIRE(ff_repo->getRepositoryEntryCount() == 0);

  CHECK(content_repo->getRepositoryEntryCount() == 1);
  flow_file.reset();  // release the claim held by the live flow file, which should trigger deletion of the content
  REQUIRE(content_repo->getRepositoryEntryCount() == 0);
}

}  // namespace org::apache::nifi::minifi::test
