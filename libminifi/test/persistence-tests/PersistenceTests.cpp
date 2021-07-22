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

#undef NDEBUG
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "FlowFileRepository.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "../../extensions/libarchive/MergeContent.h"
#include "../test/BufferReader.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "../../extensions/rocksdb-repos/DatabaseContentRepository.h"
#include "utils/gsl.h"
#include "utils/IntegrationTestUtils.h"

using Connection = minifi::Connection;
using MergeContent = minifi::processors::MergeContent;

namespace {

#ifdef WIN32
const std::string PERSISTENCETEST_FLOWFILE_CHECKPOINT_DIR = ".\\persistencetest_flowfile_checkpoint";
#else
const std::string PERSISTENCETEST_FLOWFILE_CHECKPOINT_DIR = "./persistencetest_flowfile_checkpoint";
#endif

struct TestFlow{
  TestFlow(const std::shared_ptr<core::Repository>& ff_repository, const std::shared_ptr<core::ContentRepository>& content_repo, const std::shared_ptr<core::Repository>& prov_repo,
        const std::function<std::shared_ptr<core::Processor>(utils::Identifier&)>& processorGenerator, const core::Relationship& relationshipToOutput)
      : ff_repository(ff_repository), content_repo(content_repo), prov_repo(prov_repo) {
    // setup processor
    {
      processor = processorGenerator(mainProcUUID());
      std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
      processorContext = std::make_shared<core::ProcessContext>(node, nullptr, prov_repo, ff_repository, content_repo);
    }

    // setup INPUT processor
    {
      inputProcessor = std::make_shared<core::Processor>("source", inputProcUUID());
      std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(inputProcessor);
      inputContext = std::make_shared<core::ProcessContext>(node, nullptr, prov_repo,
                                                            ff_repository, content_repo);
    }

    // setup Input Connection
    {
      input = std::make_shared<Connection>(ff_repository, content_repo, "Input", inputConnUUID());
      input->setRelationship({"input", "d"});
      input->setDestinationUUID(mainProcUUID());
      input->setSourceUUID(inputProcUUID());
      inputProcessor->addConnection(input);
    }

    // setup Output Connection
    {
      output = std::make_shared<Connection>(ff_repository, content_repo, "Output", outputConnUUID());
      output->setRelationship(relationshipToOutput);
      output->setSourceUUID(mainProcUUID());
    }

    // setup ProcessGroup
    {
      root = std::make_shared<core::ProcessGroup>(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
      root->addProcessor(processor);
      root->addConnection(input);
      root->addConnection(output);
    }

    // prepare Merge Processor for execution
    processor->setScheduledState(core::ScheduledState::RUNNING);
    processor->onSchedule(processorContext.get(), new core::ProcessSessionFactory(processorContext));
  }
  std::shared_ptr<core::FlowFile> write(const std::string& data) {
    minifi::io::BufferStream stream(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
    core::ProcessSession sessionGenFlowFile(inputContext);
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    sessionGenFlowFile.importFrom(stream, flow);
    assert(flow->getResourceClaim()->getFlowFileRecordOwnedCount() == 1);
    sessionGenFlowFile.transfer(flow, {"input", "d"});
    sessionGenFlowFile.commit();
    return flow;
  }
  std::string read(const std::shared_ptr<core::FlowFile>& file) {
    core::ProcessSession session(processorContext);
    std::vector<uint8_t> buffer;
    BufferReader reader(buffer);
    session.read(file, &reader);
    return {buffer.data(), buffer.data() + buffer.size()};
  }
  void trigger() {
    auto session = std::make_shared<core::ProcessSession>(processorContext);
    processor->onTrigger(processorContext, session);
    session->commit();
  }

  std::shared_ptr<Connection> input;
  std::shared_ptr<Connection> output;
  std::shared_ptr<core::ProcessGroup> root;

 private:
  static utils::Identifier& mainProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& outputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}

  std::shared_ptr<core::Processor> inputProcessor;
  std::shared_ptr<core::Processor> processor;
  std::shared_ptr<core::Repository> ff_repository;
  std::shared_ptr<core::ContentRepository> content_repo;
  std::shared_ptr<core::Repository> prov_repo;
  std::shared_ptr<core::ProcessContext> inputContext;
  std::shared_ptr<core::ProcessContext> processorContext;
};

std::shared_ptr<MergeContent> setupMergeProcessor(const utils::Identifier& id) {
  auto processor = std::make_shared<MergeContent>("MergeContent", id);
  processor->initialize();
  processor->setAutoTerminatedRelationships({{"original", "d"}});

  processor->setProperty(MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  processor->setProperty(MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  processor->setProperty(MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  processor->setProperty(MergeContent::MinEntries, "3");
  processor->setProperty(MergeContent::Header, "_Header_");
  processor->setProperty(MergeContent::Footer, "_Footer_");
  processor->setProperty(MergeContent::Demarcator, "_Demarcator_");
  processor->setProperty(MergeContent::MaxBinAge, "1 h");

  return processor;
}

TEST_CASE("Processors Can Store FlowFiles", "[TestP1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();
  LogTestController::getInstance().setTrace<core::repository::FlowFileRepository>();

  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, utils::file::FileUtils::concat_path(dir, "content_repository"));
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::repository::FlowFileRepository> ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository", PERSISTENCETEST_FLOWFILE_CHECKPOINT_DIR);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  ff_repository->initialize(config);
  content_repo->initialize(config);

  auto flowConfig = std::unique_ptr<core::FlowConfiguration>{new core::FlowConfiguration(prov_repo, ff_repository, content_repo, nullptr, config, "")};
  auto flowController = std::make_shared<minifi::FlowController>(
      prov_repo, ff_repository, config, std::move(flowConfig), content_repo, "");

  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupMergeProcessor, MergeContent::Merge);

    flowController->load(flow.root);
    ff_repository->start();

    // write two files into the input
    flow.write("one");
    flow.write("two");
    // capture them with the Merge Processor
    flow.trigger();
    flow.trigger();

    ff_repository->stop();
    flowController->unload();

    // check if the processor has taken ownership
    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto file = flow.input->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());

    file = flow.output->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());
  }

  // swap the ProcessGroup and restart the FlowController
  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupMergeProcessor, MergeContent::Merge);

    flowController->load(flow.root);
    ff_repository->start();
    // wait for FlowFileRepository to start and notify the owners of
    // the resurrected FlowFiles
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::seconds(1), []{ return LogTestController::getInstance().countOccurrences("Found connection for") == 2; }, std::chrono::milliseconds(100)));

    // write the third file into the input
    flow.write("three");

    flow.trigger();
    ff_repository->stop();
    flowController->unload();
    std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> file = nullptr;
    std::set<std::shared_ptr<core::FlowFile>> expired;
    const auto flowFileArrivedInOutput = [&file, &expired, &flow] {
      file = flow.output->poll(expired);
      return file != nullptr;
    };
    assert(verifyEventHappenedInPollTime(std::chrono::seconds(1), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
    REQUIRE(expired.empty());

    auto content = flow.read(file);
    // See important note about matchers at: https://github.com/catchorg/Catch2/blob/e8cdfdca87ebacd993befdd08ea6aa7e8068ef3d/docs/matchers.md#using-matchers
    REQUIRE_THAT(content, Catch::Equals("_Header_one_Demarcator_two_Demarcator_three_Footer_") || Catch::Equals("_Header_two_Demarcator_one_Demarcator_three_Footer_"));
  }
}

class ContentUpdaterProcessor : public core::Processor{
 public:
  ContentUpdaterProcessor(const std::string& name, const utils::Identifier& id) : Processor(name, id) {}
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession *session) override {
    auto ff = session->get();
    std::string data = "<override>";
    minifi::io::BufferStream stream(reinterpret_cast<const uint8_t*>(data.c_str()), gsl::narrow<int>(data.length()));
    session->importFrom(stream, ff);
    session->transfer(ff, {"success", "d"});
  }
};

std::shared_ptr<core::Processor> setupContentUpdaterProcessor(const utils::Identifier& id) {
  return std::make_shared<ContentUpdaterProcessor>("Updater", id);
}

TEST_CASE("Persisted flowFiles are updated on modification", "[TestP1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();
  LogTestController::getInstance().setTrace<core::repository::FlowFileRepository>();
  LogTestController::getInstance().setTrace<core::repository::VolatileRepository<minifi::ResourceClaim::Path>>();
  LogTestController::getInstance().setTrace<core::repository::DatabaseContentRepository>();

  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, utils::file::FileUtils::concat_path(dir, "content_repository"));
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository", PERSISTENCETEST_FLOWFILE_CHECKPOINT_DIR);
  std::shared_ptr<core::ContentRepository> content_repo;
  SECTION("VolatileContentRepository") {
    testController.getLogger()->log_info("Using VolatileContentRepository");
    content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  }
  SECTION("FileSystemContentRepository") {
    testController.getLogger()->log_info("Using FileSystemRepository");
    content_repo = std::make_shared<core::repository::FileSystemRepository>();
  }
  SECTION("DatabaseContentRepository") {
    testController.getLogger()->log_info("Using DatabaseContentRepository");
    content_repo = std::make_shared<core::repository::DatabaseContentRepository>();
  }
  ff_repository->initialize(config);
  content_repo->initialize(config);

  auto flowConfig = std::unique_ptr<core::FlowConfiguration>{new core::FlowConfiguration(prov_repo, ff_repository, content_repo, nullptr, config, "")};
  auto flowController = std::make_shared<minifi::FlowController>(
      prov_repo, ff_repository, config, std::move(flowConfig), content_repo, "");

  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupContentUpdaterProcessor, {"success", "d"});

    flowController->load(flow.root);
    ff_repository->start();

    std::string removedResource;
    {
      // write two files into the input
      auto flowFile = flow.write("data");
      auto claim = flowFile->getResourceClaim();
      removedResource = claim->getContentFullPath();
      // one from the FlowFile and one from the persisted instance
      REQUIRE(claim->getFlowFileRecordOwnedCount() == 2);
      // update them with the Merge Processor
      flow.trigger();

      auto content = flow.read(flowFile);
      REQUIRE(content == "<override>");
      auto newClaim = flowFile->getResourceClaim();
      // the processor added new content to the flowFile
      REQUIRE(claim != newClaim);
      // only this instance behind this shared_ptr keeps the resource alive
      REQUIRE(claim.use_count() == 1);
      REQUIRE(claim->getFlowFileRecordOwnedCount() == 1);
      // one from the FlowFile and one from the persisted instance
      REQUIRE(newClaim->getFlowFileRecordOwnedCount() == 2);
    }
    REQUIRE(LogTestController::getInstance().countOccurrences("Deleting resource " + removedResource) == 1);
    REQUIRE(LogTestController::getInstance().countOccurrences("Deleting resource") == 1);

    ff_repository->stop();
    flowController->unload();
  }

  // swap the ProcessGroup and restart the FlowController
  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupContentUpdaterProcessor, {"success", "d"});

    flowController->load(flow.root);
    ff_repository->start();
    // wait for FlowFileRepository to start and notify the owners of
    // the resurrected FlowFiles
    LogTestController::getInstance().contains("Found connection for");
    std::set<std::shared_ptr<core::FlowFile>> expired;
    std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> file = nullptr;
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    const auto flowFileArrivedInOutput = [&file, &expired, &flow] {
      file = flow.output->poll(expired);
      return file != nullptr;
    };
    assert(verifyEventHappenedInPollTime(std::chrono::seconds(1), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
    REQUIRE(expired.empty());

    auto content = flow.read(file);
    REQUIRE(content == "<override>");
    // the still persisted instance and this FlowFile
    REQUIRE(file->getResourceClaim()->getFlowFileRecordOwnedCount() == 2);

    ff_repository->stop();
    flowController->unload();
  }
}

}  // namespace
