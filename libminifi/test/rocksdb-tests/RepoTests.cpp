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
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "FlowFileRepository.h"
#include "provenance/Provenance.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/gsl.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

namespace {

#ifdef WIN32
const std::string REPOTEST_FLOWFILE_CHECKPOINT_DIR = ".\\repotest_flowfile_checkpoint";
#else
const std::string REPOTEST_FLOWFILE_CHECKPOINT_DIR = "./repotest_flowfile_checkpoint";
#endif

namespace {
class TestProcessor : public minifi::core::Processor {
 public:
  using Processor::Processor;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};
}  // namespace

TEST_CASE("Test Repo Names", "[TestFFR1]") {
  auto repoA = minifi::core::createRepository("FlowFileRepository", "flowfile");
  REQUIRE("flowfile" == repoA->getName());

  auto repoB = minifi::core::createRepository("ProvenanceRepository", "provenance");
  REQUIRE("provenance" == repoB->getName());
}

TEST_CASE("Test Repo Empty Value Attribute", "[TestFFR1]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  auto dir = testController.createTempDirectory();
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", REPOTEST_FLOWFILE_CHECKPOINT_DIR, dir, 0ms, 0, 1ms);

  repository->initialize(std::make_shared<minifi::Configure>());

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  auto file = std::make_shared<minifi::FlowFileRecord>();

  file->addAttribute("keyA", "");

  REQUIRE(true == file->Persist(repository));

  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);

  repository->stop();
}

TEST_CASE("Test Repo Empty Key Attribute ", "[TestFFR2]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  auto dir = testController.createTempDirectory();
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", REPOTEST_FLOWFILE_CHECKPOINT_DIR, dir, 0ms, 0, 1ms);

  repository->initialize(std::make_shared<minifi::Configure>());
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  auto file = std::make_shared<minifi::FlowFileRecord>();

  file->addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  file->addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

  REQUIRE(true == file->Persist(repository));

  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);

  repository->stop();
}

TEST_CASE("Test Repo Key Attribute Verify ", "[TestFFR3]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  auto dir = testController.createTempDirectory();
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", REPOTEST_FLOWFILE_CHECKPOINT_DIR, dir, 0ms, 0, 1ms);

  repository->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  minifi::FlowFileRecord record;

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

  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);
}

TEST_CASE("Test Delete Content ", "[TestFFR4]") {
  TestController testController;

  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", REPOTEST_FLOWFILE_CHECKPOINT_DIR, dir, 0ms, 0, 1ms);

  std::fstream file;
  std::stringstream ss;
  ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  repository->initialize(std::make_shared<minifi::Configure>());

  repository->loadComponent(content_repo);


  {
    std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ss.str(), content_repo);
    minifi::FlowFileRecord record;
    record.setResourceClaim(claim);

    record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

    record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

    REQUIRE(record.Persist(repository));

    REQUIRE(repository->Delete(record.getUUIDStr()));
    claim->decreaseFlowFileRecordOwnedCount();

    repository->flush();

    repository->stop();
  }

  std::ifstream fileopen(ss.str(), std::ios::in);
  REQUIRE(!fileopen.good());

  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);

  LogTestController::getInstance().reset();
}

TEST_CASE("Test Validate Checkpoint ", "[TestFFR5]") {
  TestController testController;
  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);

  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", REPOTEST_FLOWFILE_CHECKPOINT_DIR, dir, 0ms, 0, 1ms);

  std::fstream file;
  std::stringstream ss;
  ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  repository->initialize(std::make_shared<minifi::Configure>());

  repository->loadComponent(content_repo);

  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ss.str(), content_repo);
  {
    minifi::FlowFileRecord record;
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

  std::ifstream fileopen(ss.str(), std::ios::in);
  REQUIRE(fileopen.fail());

  utils::file::FileUtils::delete_dir(REPOTEST_FLOWFILE_CHECKPOINT_DIR, true);

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

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, utils::file::FileUtils::concat_path(dir, "content_repository"));
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestThreadedRepository>();
  auto ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository", REPOTEST_FLOWFILE_CHECKPOINT_DIR);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  ff_repository->initialize(config);
  content_repo->initialize(config);

  core::Relationship inputRel{"Input", "dummy"};
  auto input = std::make_unique<minifi::Connection>(ff_repository, content_repo, "Input");
  input->setRelationship(inputRel);

  auto root = std::make_unique<core::ProcessGroup>(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
  auto inputPtr = input.get();
  root->addConnection(std::move(input));

  auto flowConfig = std::make_unique<core::FlowConfiguration>(prov_repo, ff_repository, content_repo, nullptr, config, "");
  auto flowController = std::make_shared<minifi::FlowController>(
      prov_repo, ff_repository, config, std::move(flowConfig), content_repo, "", std::make_shared<utils::file::FileSystem>(), []{});

  std::string data = "banana";
  minifi::io::BufferStream content(data);

  /**
   * Currently it is the Connection's responsibility to persist the incoming
   * flowFiles to the FlowFileRepository. Upon restart the FlowFileRepository
   * checks the persisted database and moves every FlowFile into the Connection
   * that persisted it (if it can find it. We could have a different flow, in
   * which case the orphan FlowFiles are deleted.)
   */
  {
    std::shared_ptr<core::Processor> processor = std::make_shared<TestProcessor>("dummy");
    utils::Identifier uuid = processor->getUUID();
    REQUIRE(uuid);
    inputPtr->setSourceUUID(uuid);
    processor->addConnection(inputPtr);
    auto node = std::make_shared<core::ProcessorNode>(processor.get());
    auto context = std::make_shared<core::ProcessContext>(node, nullptr, prov_repo, ff_repository, content_repo);
    core::ProcessSession sessionGenFlowFile(context);
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
  LogTestController::getInstance().contains("Found connection for");

  // check if the @input Connection's FlowFile was restored
  // upon the FlowFileRepository's startup
  std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> newFlow = nullptr;
  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  const auto flowFileArrivedInOutput = [&newFlow, &expiredFiles, inputPtr] {
    newFlow = inputPtr->poll(expiredFiles);
    return newFlow != nullptr;
  };
  assert(verifyEventHappenedInPollTime(std::chrono::seconds(10), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
  (void)flowFileArrivedInOutput;  // unused in release builds
  REQUIRE(expiredFiles.empty());

  LogTestController::getInstance().reset();
}

TEST_CASE("Flush deleted flowfiles before shutdown", "[TestFFR7]") {
  class TestFlowFileRepository: public core::repository::FlowFileRepository{
   public:
    explicit TestFlowFileRepository(const std::string& name)
        : core::SerializableComponent(name),
          FlowFileRepository(name, REPOTEST_FLOWFILE_CHECKPOINT_DIR, core::repository::FLOWFILE_REPOSITORY_DIRECTORY,
                             10min, core::repository::MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, 1ms) {}
    void flush() override {
      FlowFileRepository::flush();
      if (onFlush_) {
        onFlush_();
      }
    }
    std::function<void()> onFlush_;
  };

  TestController testController;
  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  auto connection = std::make_shared<minifi::Connection>(nullptr, nullptr, "Connection");
  std::map<std::string, core::Connectable*> connectionMap{{connection->getUUIDStr(), connection.get()}};
  // initialize repository
  {
    std::shared_ptr<TestFlowFileRepository> ff_repository = std::make_shared<TestFlowFileRepository>("flowFileRepository");

    std::atomic<int> flush_counter{0};

    std::atomic<bool> stop{false};
    std::thread shutdown{[&] {
      while (!stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      ff_repository->stop();
    }};

    ff_repository->onFlush_ = [&] {
      if (++flush_counter != 1) {
        return;
      }

      for (int keyIdx = 0; keyIdx < 100; ++keyIdx) {
        auto file = std::make_shared<minifi::FlowFileRecord>();
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

    shutdown.join();
  }

  // check if the deleted flowfiles are indeed deleted
  {
    std::shared_ptr<TestFlowFileRepository> ff_repository = std::make_shared<TestFlowFileRepository>("flowFileRepository");
    ff_repository->setConnectionMap(connectionMap);
    REQUIRE(ff_repository->initialize(config));
    ff_repository->loadComponent(content_repo);
    ff_repository->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&connection]{ return connection->getQueueSize() == 50; }, std::chrono::milliseconds(50)));
  }
}

}  // namespace
