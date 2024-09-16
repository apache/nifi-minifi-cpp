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

#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <optional>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "FlowFileRepository.h"
#include "ProvenanceRepository.h"
#include "provenance/Provenance.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/gsl.h"
#include "unit/TestUtils.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "core/repository/VolatileProvenanceRepository.h"
#include "DatabaseContentRepository.h"
#include "catch2/generators/catch_generators.hpp"
#include "core/repository/FileSystemRepository.h"
#include "core/ProcessorNode.h"

using namespace std::literals::chrono_literals;

namespace {

namespace {
class TestProcessor final : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};
}  // namespace

TEST_CASE("Test Repo Names", "[TestFFR1]") {
  const auto repoA = minifi::core::createRepository("FlowFileRepository", "flowfile");
  REQUIRE("flowfile" == repoA->getName());

  const auto repoB = minifi::core::createRepository("ProvenanceRepository", "provenance");
  REQUIRE("provenance" == repoB->getName());
}

TEST_CASE("Test Repo Empty Value Attribute", "[TestFFR1]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  const auto dir = testController.createTempDirectory();
  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  const auto file = std::make_shared<minifi::FlowFileRecordImpl>();

  file->addAttribute("keyA", "");

  REQUIRE(true == file->Persist(repository));

  repository->stop();
}

TEST_CASE("Test Repo Empty Key Attribute ", "[TestFFR2]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  const auto dir = testController.createTempDirectory();
  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  const auto file = std::make_shared<minifi::FlowFileRecordImpl>();

  file->addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  file->addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

  REQUIRE(true == file->Persist(repository));

  repository->stop();
}

TEST_CASE("Test Repo Key Attribute Verify ", "[TestFFR3]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  auto dir = testController.createTempDirectory();
  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  minifi::FlowFileRecordImpl record;

  std::string uuid = record.getUUIDStr();

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("keyB", "");

  record.addAttribute("", "");

  record.updateAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd2");

  record.addAttribute("", "sdgsdg");

  REQUIRE(record.Persist(repository));

  repository->stop();

  utils::Identifier containerId;
  auto record2 = minifi::FlowFileRecord::DeSerialize(uuid, repository, content_repo, containerId);

  std::string value;
  REQUIRE(record2->getAttribute("", value));

  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd2" == value);

  REQUIRE(!record2->getAttribute("key", value));
  REQUIRE(record2->getAttribute("keyA", value));
  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd" == value);

  REQUIRE(record2->getAttribute("keyB", value));
  REQUIRE(value.empty());
}

TEST_CASE("Test Delete Content ", "[TestFFR4]") {
  TestController testController;

  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();

  auto dir = testController.createTempDirectory();

  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  std::fstream file;
  file.open(dir / "tstFile.ext", std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  repository->loadComponent(content_repo);


  {
    const auto claim = std::make_shared<minifi::ResourceClaimImpl>((dir / "tstFile.ext").string(), content_repo);
    minifi::FlowFileRecordImpl record;
    record.setResourceClaim(claim);

    record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

    record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

    REQUIRE(record.Persist(repository));

    REQUIRE(repository->Delete(record.getUUIDStr()));
    claim->decreaseFlowFileRecordOwnedCount();

    repository->flush();

    repository->stop();
  }

  std::ifstream fileopen(dir / "tstFile.ext", std::ios::in);
  REQUIRE(!fileopen.good());

  LogTestController::getInstance().reset();
}

TEST_CASE("Test Validate Checkpoint ", "[TestFFR5]") {
  TestController testController;

  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();

  auto dir = testController.createTempDirectory();

  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  std::fstream file;
  file.open(dir / "tstFile.ext", std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  repository->loadComponent(content_repo);

  auto claim = std::make_shared<minifi::ResourceClaimImpl>((dir / "tstFile.ext").string(), content_repo);

  {
    minifi::FlowFileRecordImpl record;
    record.setResourceClaim(claim);

    record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

    record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

    REQUIRE(record.Persist(repository));

    repository->flush();

    repository->stop();

    content_repo->reset();

    repository->loadComponent(content_repo);

    repository->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    repository->stop();
    claim = nullptr;
    // sleep for 100 ms to let the delete work.

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::ifstream fileopen(dir / "tstFile.ext", std::ios::in);
  REQUIRE(fileopen.fail());

  LogTestController::getInstance().reset();
}

TEST_CASE("Test FlowFile Restore", "[TestFFR6]") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();
  LogTestController::getInstance().setTrace<minifi::core::repository::FlowFileRepository>();

  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(dir);
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, (dir / "content_repository").string());
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, (dir / "flowfile_repository").string());

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestThreadedRepository>();
  auto ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  ff_repository->initialize(config);
  content_repo->initialize(config);

  core::Relationship inputRel{"Input", "dummy"};
  auto input = std::make_unique<minifi::ConnectionImpl>(ff_repository, content_repo, "Input");
  input->addRelationship(inputRel);

  auto root = std::make_unique<core::ProcessGroup>(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
  auto inputPtr = input.get();
  root->addConnection(std::move(input));

  auto flowConfig = std::make_unique<core::FlowConfiguration>(core::ConfigurationContext{
      .flow_file_repo = ff_repository,
      .content_repo = content_repo,
      .configuration = config,
      .path = "",
      .filesystem = std::make_shared<utils::file::FileSystem>(),
      .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  auto flowController = std::make_shared<minifi::FlowController>(prov_repo, ff_repository, config, std::move(flowConfig), content_repo);

  /**
   * Currently it is the Connection's responsibility to persist the incoming
   * flowFiles to the FlowFileRepository. Upon restart the FlowFileRepository
   * checks the persisted database and moves every FlowFile into the Connection
   * that persisted it (if it can find it. We could have a different flow, in
   * which case the orphan FlowFiles are deleted.)
   */
  {
    std::string data = "banana";
    minifi::io::BufferStream content(data);
    std::shared_ptr<core::Processor> processor = std::make_shared<TestProcessor>("dummy");
    utils::Identifier uuid = processor->getUUID();
    REQUIRE(uuid);
    inputPtr->setSourceUUID(uuid);
    processor->addConnection(inputPtr);
    auto node = std::make_shared<core::ProcessorNodeImpl>(processor.get());
    auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, prov_repo, ff_repository, content_repo);
    core::ProcessSessionImpl sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    sessionGenFlowFile.importFrom(content, flow);
    sessionGenFlowFile.transfer(flow, inputRel);
    sessionGenFlowFile.commit();
  }

  // remove flow from the connection but it is still present in the
  // flowFileRepo
  std::set<std::shared_ptr<core::FlowFile>> expiredFiles;
  auto oldFlow = inputPtr->poll(expiredFiles);
  REQUIRE(oldFlow);
  REQUIRE(expiredFiles.empty());

  // this notifies the FlowFileRepository of the flow structure
  // i.e. what Connections are present (more precisely what Connectables
  // are present)
  flowController->load(std::move(root));
  // this will first check the persisted repo and restore all FlowFiles
  // that still has an owner Connectable
  ff_repository->start();
  CHECK(LogTestController::getInstance().contains("Found connection for"));

  // check if the @input Connection's FlowFile was restored
  // upon the FlowFileRepository's startup
  std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> newFlow = nullptr;
  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  const auto flowFileArrivedInOutput = [&newFlow, &expiredFiles, inputPtr] {
    newFlow = inputPtr->poll(expiredFiles);
    return newFlow != nullptr;
  };
  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(10), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
  REQUIRE(expiredFiles.empty());

  LogTestController::getInstance().reset();
}

TEST_CASE("Flush deleted flowfiles before shutdown", "[TestFFR7]") {
  class TestFlowFileRepository: public core::repository::FlowFileRepository{
   public:
    explicit TestFlowFileRepository(const std::string& name)
      : FlowFileRepository(name, core::repository::FLOWFILE_REPOSITORY_DIRECTORY,
                           10min, core::repository::MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, 50ms) {}

    void flush() override {
      FlowFileRepository::flush();
      if (onFlush_) {
        onFlush_();
      }
    }
    std::function<void()> onFlush_;
  };

  TestController testController;
  const auto dir = testController.createTempDirectory();

  const auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(dir);
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, (dir / "flowfile_repository").string());

  const auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  auto connection = std::make_shared<minifi::ConnectionImpl>(nullptr, nullptr, "Connection");
  std::map<std::string, core::Connectable*> connectionMap{{connection->getUUIDStr(), connection.get()}};
  // initialize repository
  {
    std::mutex flush_counter_mutex;
    int flush_counter{0};
    std::atomic<bool> stop{false};

    const auto ff_repository = std::make_shared<TestFlowFileRepository>("flowFileRepository");

    std::thread shutdown{[&] {
      while (!stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      ff_repository->stop();
    }};

    ff_repository->onFlush_ = [&] {
      {
        std::lock_guard lock(flush_counter_mutex);
        if (++flush_counter != 1) {
          return;
        }
      }

      for (int keyIdx = 0; keyIdx < 100; ++keyIdx) {
        auto file = std::make_shared<minifi::FlowFileRecordImpl>();
        file->setConnection(connection.get());
        // Serialize is sync
        REQUIRE(file->Persist(ff_repository));
        if (keyIdx % 2 == 0) {
          // delete every second flowFile
          REQUIRE(ff_repository->Delete(file->getUUIDStr()));
        }
      }
      stop = true;
      // wait for the shutdown thread to start waiting for the worker thread
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    };

    ff_repository->setConnectionMap(connectionMap);
    REQUIRE(ff_repository->initialize(config));
    ff_repository->loadComponent(content_repo);
    ff_repository->start();

    if (shutdown.joinable()) {
      shutdown.join();
    }
  }

  // check if the deleted flowfiles are indeed deleted
  {
    const auto ff_repository = std::make_shared<TestFlowFileRepository>("flowFileRepository");
    ff_repository->setConnectionMap(connectionMap);
    REQUIRE(ff_repository->initialize(config));
    ff_repository->loadComponent(content_repo);
    ff_repository->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&connection]{ return connection->getQueueSize() == 50; }, std::chrono::milliseconds(50)));
  }
}

TEST_CASE("FlowFileRepository triggers content repo orphan clear") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  auto home_dir = testController.createTempDirectory();
  auto ff_dir = testController.createTempDirectory();
  auto content_dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(home_dir);
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  {
    auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
    REQUIRE(content_repo->initialize(config));
    minifi::ResourceClaimImpl claim(content_repo);
    content_repo->write(claim)->write("hi");
    // ensure that the content is not deleted during resource claim destruction
    content_repo->incrementStreamCount(claim);
  }

  REQUIRE(utils::file::list_dir_all(content_dir, testController.getLogger()).size() == 1);

  auto ff_repo = std::make_shared<core::repository::FlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));
  auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
  REQUIRE(content_repo->initialize(config));

  ff_repo->loadComponent(content_repo);

  REQUIRE(utils::file::list_dir_all(content_dir, testController.getLogger()).empty());
}

TEST_CASE("FlowFileRepository synchronously pushes existing flow files") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  const auto home_dir = testController.createTempDirectory();
  const auto ff_dir = testController.createTempDirectory();
  const auto content_dir = testController.createTempDirectory();

  const auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(home_dir);
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());


  utils::Identifier ff_id;
  const auto connection_id = utils::IdGenerator::getIdGenerator()->generate();

  {
    auto ff_repo = std::make_shared<core::repository::FlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
    REQUIRE(content_repo->initialize(config));
    auto conn = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

    auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo);

    std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>> flow_data;
    auto ff = std::make_shared<minifi::FlowFileRecordImpl>();
    ff_id = ff->getUUID();
    ff->setConnection(conn.get());
    content_repo->write(*claim)->write("hello");
    ff->setResourceClaim(claim);
    auto stream = std::make_unique<minifi::io::BufferStream>();
    ff->Serialize(*stream);
    flow_data.emplace_back(ff->getUUIDStr(), std::move(stream));

    REQUIRE(ff_repo->MultiPut(flow_data));
  }

  {
    auto ff_repo = std::make_shared<core::repository::FlowFileRepository>();
    REQUIRE(ff_repo->initialize(config));
    auto content_repo = std::make_shared<core::repository::FileSystemRepository>();
    REQUIRE(content_repo->initialize(config));
    auto conn = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

    ff_repo->setConnectionMap({{connection_id.to_string(), conn.get()}});
    ff_repo->loadComponent(content_repo);

    std::set<std::shared_ptr<core::FlowFile>> expired;
    std::shared_ptr<core::FlowFile> ff = conn->poll(expired);
    REQUIRE(expired.empty());
    REQUIRE(ff);
    REQUIRE(ff->getUUID() == ff_id);
  }
}

TEST_CASE("Test getting flow file repository size properties", "[TestGettingRepositorySize]") {
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  LogTestController::getInstance().setDebug<minifi::provenance::ProvenanceRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileFlowFileRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileProvenanceRepository>();
  TestController testController;
  auto dir = testController.createTempDirectory();

  std::shared_ptr<core::Repository> repository;
  auto expected_is_full = false;
  uint64_t expected_max_repo_size = 0;
  bool expected_rocksdb_stats = false;
  SECTION("FlowFileRepository") {
    repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);
    expected_rocksdb_stats = true;
  }

  SECTION("ProvenanceRepository") {
    repository = std::make_shared<minifi::provenance::ProvenanceRepository>("ff", dir.string(), 0ms, 0, 1ms);
    expected_rocksdb_stats = true;
  }

  SECTION("VolatileFlowFileRepository") {
    repository = std::make_shared<core::repository::VolatileFlowFileRepository>("ff", dir.string(), 0ms, 10, 1ms);
    expected_is_full = true;
    expected_max_repo_size = 7;
  }

  SECTION("VolatileProvenanceRepository") {
    repository = std::make_shared<core::repository::VolatileProvenanceRepository>("ff", dir.string(), 0ms, 10, 1ms);
    expected_is_full = true;
    expected_max_repo_size = 7;
  }
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();

  for (auto i = 0; i < 100; ++i) {
    flow_file->addAttribute("key" + std::to_string(i), "testattributevalue" + std::to_string(i));
  }

  auto original_size = repository->getRepositorySize();
  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(5), [&original_size, &repository] {
      auto old_size = original_size;
      original_size = repository->getRepositorySize();
      return old_size == original_size;
    },
    std::chrono::milliseconds(50)));
  REQUIRE(true == flow_file->Persist(repository));
  auto flow_file_2 = std::make_shared<minifi::FlowFileRecordImpl>();
  REQUIRE(true == flow_file_2->Persist(repository));

  repository->flush();
  repository->stop();

  auto new_size = repository->getRepositorySize();
  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(5), [&new_size, &repository] {
      auto old_size = new_size;
      new_size = repository->getRepositorySize();
      return old_size == new_size;
    },
    std::chrono::milliseconds(50)));
  REQUIRE(new_size > original_size);
  REQUIRE(expected_is_full == repository->isFull());
  REQUIRE(expected_max_repo_size == repository->getMaxRepositorySize());
  REQUIRE(2 == repository->getRepositoryEntryCount());
  auto rocksdb_stats = repository->getRocksDbStats();
  REQUIRE(expected_rocksdb_stats == (rocksdb_stats != std::nullopt));
  if (rocksdb_stats) {
    REQUIRE(rocksdb_stats->all_memory_tables_size > 0);
  }
}

TEST_CASE("Test getting noop repository size properties", "[TestGettingRepositorySize]") {
  TestController testController;
  const auto dir = testController.createTempDirectory();

  const auto repository = minifi::core::createRepository("NoOpRepository", "ff");

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  const auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();

  flow_file->addAttribute("key", "testattributevalue");

  repository->flush();
  repository->stop();

  REQUIRE(repository->getRepositorySize() == 0);
  REQUIRE(!repository->isFull());
  REQUIRE(repository->getMaxRepositorySize() == 0);
  REQUIRE(repository->getRepositoryEntryCount() == 0);
}

TEST_CASE("Test getting content repository size properties", "[TestGettingRepositorySize]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::DatabaseContentRepository>();
  TestController testController;
  const auto dir = testController.createTempDirectory();

  const auto repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);

  const auto content_repo_dir = testController.createTempDirectory();
  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_dir.string());
  std::string content = "content";
  configuration->set(minifi::Configure::nifi_volatile_repository_options_content_max_bytes, std::to_string(content.size()));

  std::shared_ptr<core::ContentRepository> content_repo;
  auto expected_is_full = false;
  uint64_t expected_max_repo_size = 0;
  bool expected_rocksdb_stats = false;
  SECTION("FileSystemRepository") {
    content_repo = std::make_shared<core::repository::FileSystemRepository>();
  }

  SECTION("VolatileContentRepository") {
    content_repo = std::make_shared<core::repository::VolatileContentRepository>("content");
    expected_is_full = true;
    expected_max_repo_size = content.size();
  }

  SECTION("DatabaseContentRepository") {
    content_repo = std::make_shared<core::repository::DatabaseContentRepository>();
    expected_rocksdb_stats = true;
  }

  content_repo->initialize(configuration);

  repository->initialize(configuration);
  repository->loadComponent(content_repo);
  auto original_content_repo_size = content_repo->getRepositorySize();

  auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();

  auto content_session = content_repo->createSession();
  auto claim = content_session->create();
  auto stream = content_session->write(claim);
  stream->write(gsl::make_span(content).as_span<const std::byte>());
  flow_file->setResourceClaim(claim);
  flow_file->setSize(stream->size());
  flow_file->setOffset(0);

  stream->close();
  content_session->commit();

  repository->flush();
  repository->stop();

  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(5), [&original_content_repo_size, &content_repo] {
      auto new_content_repo_size = content_repo->getRepositorySize();
      return new_content_repo_size > original_content_repo_size;
    },
    std::chrono::milliseconds(50)));

  REQUIRE(expected_is_full == content_repo->isFull());
  REQUIRE(expected_max_repo_size == content_repo->getMaxRepositorySize());
  REQUIRE(1 == content_repo->getRepositoryEntryCount());
  auto rocksdb_stats = content_repo->getRocksDbStats();
  REQUIRE(expected_rocksdb_stats == (rocksdb_stats != std::nullopt));
  if (rocksdb_stats) {
    REQUIRE(rocksdb_stats->all_memory_tables_size > 0);
  }
}

TEST_CASE("Flow file repositories can be stopped", "[TestRepoIsRunning]") {
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  LogTestController::getInstance().setDebug<minifi::provenance::ProvenanceRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileFlowFileRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileProvenanceRepository>();
  TestController testController;
  const auto dir = testController.createTempDirectory();

  std::shared_ptr<core::Repository> repository;
  SECTION("FlowFileRepository") {
    repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir.string(), 0ms, 0, 1ms);
  }

  SECTION("ProvenanceRepository") {
    repository = std::make_shared<minifi::provenance::ProvenanceRepository>("ff", dir.string(), 0ms, 0, 1ms);
  }

  SECTION("VolatileFlowFileRepository") {
    repository = std::make_shared<core::repository::VolatileFlowFileRepository>("ff", dir.string(), 0ms, 10, 1ms);
  }

  SECTION("VolatileProvenanceRepository") {
    repository = std::make_shared<core::repository::VolatileProvenanceRepository>("ff", dir.string(), 0ms, 10, 1ms);
  }

  SECTION("NoOpRepository") {
    repository = core::createRepository("NoOpRepository", "ff");
  }

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(dir);
  repository->initialize(configuration);

  REQUIRE(!repository->isRunning());
  repository->start();
  REQUIRE(repository->isRunning());
  repository->stop();
  REQUIRE(!repository->isRunning());
}

TEST_CASE("Content repositories are always running", "[TestRepoIsRunning]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::DatabaseContentRepository>();
  TestController testController;
  const auto dir = testController.createTempDirectory();

  std::shared_ptr<core::ContentRepository> content_repo;
  SECTION("FileSystemRepository") {
    content_repo = std::make_shared<core::repository::FileSystemRepository>();
  }

  SECTION("VolatileContentRepository") {
    content_repo = std::make_shared<core::repository::VolatileContentRepository>("content");
  }

  SECTION("DatabaseContentRepository") {
    content_repo = std::make_shared<core::repository::DatabaseContentRepository>();
  }

  REQUIRE(content_repo->isRunning());
}

std::shared_ptr<minifi::FlowFileRecord> createFlowFileWithContent(core::ContentRepository& content_repo, std::string_view content) {
  auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();
  const auto content_session = content_repo.createSession();
  const auto claim = content_session->create();
  const auto stream = content_session->write(claim);

  stream->write(utils::as_span<const std::byte>(std::span(content)));
  flow_file->setResourceClaim(claim);
  flow_file->setSize(stream->size());
  flow_file->setOffset(0);
  stream->close();
  content_session->commit();
  return flow_file;
}

void corruptFlowFile(core::FlowFile& ff) {
  ff.setSize(ff.getSize()*2);
}

TEST_CASE("FlowFileRepository can filter out too small contents") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  const auto minifi_home = testController.createTempDirectory();
  const auto ff_dir = testController.createTempDirectory();
  const auto content_dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(minifi_home);
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, ff_dir.string());
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, content_dir.string());

  size_t expected_flowfiles = std::numeric_limits<size_t>::max();
  std::shared_ptr<core::ContentRepository> content_repo;

  // Sections are used instead of GENERATE macro to have the content_repo destructed before testController and be able to delete the temp directories
  SECTION("nifi.flowfile.repository.check.health set to false") {
    config->set(minifi::Configure::nifi_flow_file_repository_check_health, "false");
    expected_flowfiles = 2;
    SECTION("FileSystemRepository") {
      content_repo = std::make_shared<core::repository::FileSystemRepository>();
    }
    SECTION("RocksDB") {
      content_repo = std::make_shared<core::repository::DatabaseContentRepository>();
    }
  }
  SECTION("nifi.flowfile.repository.check.health set to true") {
    config->set(minifi::Configure::nifi_flow_file_repository_check_health, "true");
    expected_flowfiles = 1;
    SECTION("FileSystemRepository") {
      content_repo = std::make_shared<core::repository::FileSystemRepository>();
    }
    SECTION("RocksDB") {
      content_repo = std::make_shared<core::repository::DatabaseContentRepository>();
    }
  }

  auto ff_repo = std::make_shared<core::repository::FlowFileRepository>();
  REQUIRE(ff_repo->initialize(config));
  REQUIRE(content_repo->initialize(config));
  auto connection_id = utils::IdGenerator::getIdGenerator()->generate();
  auto connection = std::make_shared<minifi::ConnectionImpl>(ff_repo, content_repo, "TestConnection", connection_id);

  auto first_flow_file = createFlowFileWithContent(*content_repo, "foo");
  auto second_flow_file = createFlowFileWithContent(*content_repo, "bar");

  std::map<std::string, core::Connectable*> connection_map{{connection->getUUIDStr(), connection.get()}};

  first_flow_file->setConnection(connection.get());
  first_flow_file->Persist(ff_repo);

  second_flow_file->setConnection(connection.get());
  corruptFlowFile(*second_flow_file);
  second_flow_file->Persist(ff_repo);

  ff_repo->setConnectionMap(connection_map);

  CHECK(connection->getQueueSize() == 0);
  ff_repo->loadComponent(content_repo);
  CHECK(connection->getQueueSize() == expected_flowfiles);
}

}  // namespace
