/**
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

#include <array>
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
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "../../extensions/libarchive/MergeContent.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "../../extensions/rocksdb-repos/DatabaseContentRepository.h"
#include "unit/TestUtils.h"
#include "core/ProcessorNode.h"
#include "core/repository/FileSystemRepository.h"

using ConnectionImpl = minifi::ConnectionImpl;
using Connection = minifi::Connection;
using MergeContent = minifi::processors::MergeContent;

using minifi::test::utils::verifyEventHappenedInPollTime;

namespace {

class TestProcessor : public minifi::core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

struct TestFlow{
  TestFlow(const std::shared_ptr<core::Repository>& ff_repository, const std::shared_ptr<core::ContentRepository>& content_repo, const std::shared_ptr<core::Repository>& prov_repo,
        const std::function<std::unique_ptr<core::Processor>(utils::Identifier&)>& processorGenerator, const core::Relationship& relationshipToOutput)
      : ff_repository(ff_repository), content_repo(content_repo), prov_repo(prov_repo) {
    // setup processor
    auto processor = processorGenerator(mainProcUUID());
    {
      processor_ = processor.get();
      auto node = std::make_shared<core::ProcessorNodeImpl>(processor_);
      processorContext = std::make_shared<core::ProcessContextImpl>(node, nullptr, prov_repo, ff_repository, content_repo);
    }

    // setup INPUT processor
    {
      inputProcessor = std::make_shared<TestProcessor>("source", inputProcUUID());
      auto node = std::make_shared<core::ProcessorNodeImpl>(inputProcessor.get());
      inputContext = std::make_shared<core::ProcessContextImpl>(node, nullptr, prov_repo,
                                                            ff_repository, content_repo);
    }

    // setup Input Connection
    auto input = std::make_unique<ConnectionImpl>(ff_repository, content_repo, "Input", inputConnUUID());
    {
      input_ = input.get();
      input->addRelationship({"input", "d"});
      input->setDestinationUUID(mainProcUUID());
      input->setSourceUUID(inputProcUUID());
      inputProcessor->addConnection(input.get());
    }

    // setup Output Connection
    auto output = std::make_unique<ConnectionImpl>(ff_repository, content_repo, "Output", outputConnUUID());
    {
      output_ = output.get();
      output->addRelationship(relationshipToOutput);
      output->setSourceUUID(mainProcUUID());
    }

    // setup ProcessGroup
    {
      root_ = std::make_unique<core::ProcessGroup>(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
      root_->addProcessor(std::move(processor));
      root_->addConnection(std::move(input));
      root_->addConnection(std::move(output));
    }

    // prepare Merge Processor for execution
    processor_->setScheduledState(core::ScheduledState::RUNNING);
    process_session_factory_ = std::make_unique<core::ProcessSessionFactoryImpl>(processorContext);
    processor_->onSchedule(*processorContext, *process_session_factory_);
  }
  std::shared_ptr<core::FlowFile> write(const std::string& data) {
    minifi::io::BufferStream stream(data);
    core::ProcessSessionImpl sessionGenFlowFile(inputContext);
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    sessionGenFlowFile.importFrom(stream, flow);
    REQUIRE(flow->getResourceClaim()->getFlowFileRecordOwnedCount() == 1);
    sessionGenFlowFile.transfer(flow, {"input", "d"});
    sessionGenFlowFile.commit();
    return flow;
  }
  std::string read(const std::shared_ptr<core::FlowFile>& file) {
    return to_string(core::ProcessSessionImpl{processorContext}.readBuffer(file));
  }
  void trigger() {
    auto session = std::make_shared<core::ProcessSessionImpl>(processorContext);
    processor_->onTrigger(*processorContext, *session);
    session->commit();
  }

  Connection* input_;
  Connection* output_;
  std::unique_ptr<core::ProcessGroup> root_;

 private:
  static utils::Identifier& mainProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& outputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}

  std::shared_ptr<core::Processor> inputProcessor;
  core::Processor* processor_;
  std::shared_ptr<core::Repository> ff_repository;
  std::shared_ptr<core::ContentRepository> content_repo;
  std::shared_ptr<core::Repository> prov_repo;
  std::shared_ptr<core::ProcessContext> inputContext;
  std::shared_ptr<core::ProcessContext> processorContext;
  std::unique_ptr<core::ProcessSessionFactory> process_session_factory_;
};

std::unique_ptr<MergeContent> setupMergeProcessor(const utils::Identifier& id) {
  auto processor = std::make_unique<MergeContent>("MergeContent", id);
  processor->initialize();
  processor->setAutoTerminatedRelationships(std::array{core::Relationship{"original", "d"}});

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

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(dir);
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, (dir / "content_repository").string());
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, (dir / "flowfile_repository").string());

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestThreadedRepository>();
  auto ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  ff_repository->initialize(config);
  content_repo->initialize(config);

  auto flowConfig = std::make_unique<core::FlowConfiguration>(core::ConfigurationContext{
      .flow_file_repo = ff_repository,
      .content_repo = content_repo,
      .configuration = config,
      .path = "",
      .filesystem = std::make_shared<utils::file::FileSystem>(),
      .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  auto flowController = std::make_shared<minifi::FlowController>(prov_repo, ff_repository, config, std::move(flowConfig), content_repo);

  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupMergeProcessor, MergeContent::Merge);

    flowController->load(std::move(flow.root_));
    ff_repository->start();
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&ff_repository]{ return ff_repository->isRunning(); }));

    // write two files into the input
    flow.write("one");
    flow.write("two");
    // capture them with the Merge Processor
    flow.trigger();
    flow.trigger();

    ff_repository->stop();
    flowController->stop();

    // check if the processor has taken ownership
    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto file = flow.input_->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());

    file = flow.output_->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());
  }

  // swap the ProcessGroup and restart the FlowController
  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupMergeProcessor, MergeContent::Merge);

    flowController->load(std::move(flow.root_));
    ff_repository->start();
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&ff_repository]{ return ff_repository->isRunning(); }));
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), []{ return LogTestController::getInstance().countOccurrences("Found connection for") == 2; }));

    // write the third file into the input
    flow.write("three");

    flow.trigger();
    ff_repository->stop();
    flowController->stop();
    std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> file = nullptr;
    std::set<std::shared_ptr<core::FlowFile>> expired;
    const auto flowFileArrivedInOutput = [&file, &expired, &flow] {
      file = flow.output_->poll(expired);
      return file != nullptr;
    };
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
    REQUIRE(expired.empty());

    auto content = flow.read(file);
    // See important note about matchers at: https://github.com/catchorg/Catch2/blob/e8cdfdca87ebacd993befdd08ea6aa7e8068ef3d/docs/matchers.md#using-matchers
    REQUIRE_THAT(content, Catch::Matchers::Equals("_Header_one_Demarcator_two_Demarcator_three_Footer_") || Catch::Matchers::Equals("_Header_two_Demarcator_one_Demarcator_three_Footer_"));
  }
}

class ContentUpdaterProcessor : public core::ProcessorImpl {
 public:
  ContentUpdaterProcessor(std::string_view name, const utils::Identifier& id) : ProcessorImpl(name, id) {}

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext&, core::ProcessSession& session) override {
    auto ff = session.get();
    std::string data = "<override>";
    minifi::io::BufferStream stream(data);
    session.importFrom(stream, ff);
    session.transfer(ff, {"success", "d"});
  }
};

std::unique_ptr<core::Processor> setupContentUpdaterProcessor(const utils::Identifier& id) {
  return std::make_unique<ContentUpdaterProcessor>("Updater", id);
}

TEST_CASE("Persisted flowFiles are updated on modification", "[TestP1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();
  LogTestController::getInstance().setTrace<core::repository::FlowFileRepository>();
  LogTestController::getInstance().setTrace<core::repository::DatabaseContentRepository>();

  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->setHome(dir);
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, (dir / "content_repository").string());
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, (dir / "flowfile_repository").string());
  config->set(minifi::Configure::nifi_dbcontent_repository_purge_period, "0 s");

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::Repository> ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
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

  auto flowConfig = std::make_unique<core::FlowConfiguration>(core::ConfigurationContext{
      .flow_file_repo = ff_repository,
      .content_repo = content_repo,
      .configuration = config,
      .path = "",
      .filesystem = std::make_shared<utils::file::FileSystem>(),
      .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  auto flowController = std::make_shared<minifi::FlowController>(prov_repo, ff_repository, config, std::move(flowConfig), content_repo);

  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupContentUpdaterProcessor, {"success", "d"});

    flowController->load(std::move(flow.root_));
    ff_repository->start();
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&ff_repository]{ return ff_repository->isRunning(); }));

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
    flowController->stop();
  }

  // swap the ProcessGroup and restart the FlowController
  {
    TestFlow flow(ff_repository, content_repo, prov_repo, setupContentUpdaterProcessor, {"success", "d"});

    flowController->load(std::move(flow.root_));
    ff_repository->start();
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), [&ff_repository]{ return ff_repository->isRunning(); }));

    std::set<std::shared_ptr<core::FlowFile>> expired;
    std::shared_ptr<org::apache::nifi::minifi::core::FlowFile> file = nullptr;
    const auto flowFileArrivedInOutput = [&file, &expired, &flow] {
      file = flow.output_->poll(expired);
      return file != nullptr;
    };
    REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(1), flowFileArrivedInOutput, std::chrono::milliseconds(50)));
    REQUIRE(expired.empty());

    auto content = flow.read(file);
    REQUIRE(content == "<override>");
    // the still persisted instance and this FlowFile
    REQUIRE(file->getResourceClaim()->getFlowFileRecordOwnedCount() == 2);

    ff_repository->stop();
    flowController->stop();
  }
}

}  // namespace
