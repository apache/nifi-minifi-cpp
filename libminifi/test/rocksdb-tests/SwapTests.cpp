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

#include "../Catch.h"
#include "core/RepositoryFactory.h"
#include "core/repository/VolatileContentRepository.h"
#include "FlowFileRepository.h"
#include "../TestBase.h"
#include "../Utils.h"
#include "StreamPipe.h"
#include "IntegrationTestUtils.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/RelationshipDefinition.h"
#include "../unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::test {

class OutputProcessor : public core::Processor {
 public:
  using core::Processor::Processor;
  using core::Processor::onTrigger;

  static constexpr const char* Description = "Processor used for testing cycles";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Success = core::RelationshipDefinition{"success", ""};
  static constexpr auto Relationships = std::array{Success};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(Properties);
    setSupportedRelationships(Relationships);
  }

  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) override {
    auto id = std::to_string(next_id_++);
    auto ff = session->create();
    ff->addAttribute("index", id);
    session->write(ff, [&] (const std::shared_ptr<minifi::io::OutputStream>& output) -> int64_t {
      auto ret = output->write(as_bytes(std::span(id)));
      if (minifi::io::isError(ret)) {
        return -1;
      }
      return gsl::narrow<int64_t>(ret);
    });
    session->transfer(ff, Success);
    flow_files_.push_back(ff);
  }

  std::vector<std::shared_ptr<core::FlowFile>> flow_files_;
  int next_id_{0};
};

TEST_CASE("Connection will on-demand swap flow files") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();
  LogTestController::getInstance().setTrace<minifi::utils::FlowFileQueue>();
  LogTestController::getInstance().setTrace<minifi::FlowFileLoader>();
  LogTestController::getInstance().setTrace<core::repository::FlowFileRepository>();

  auto dir = testController.createTempDirectory();

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, (dir / "content_repository").string());
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, (dir / "flowfile_repository").string());

  auto prov_repo = std::make_shared<TestRepository>();
  auto ff_repo = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
  auto swap_manager = std::dynamic_pointer_cast<minifi::SwapManager>(ff_repo);
  auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  ff_repo->initialize(config);
  content_repo->initialize(config);

  ff_repo->loadComponent(content_repo);
  ff_repo->start();

  auto processor = std::make_shared<OutputProcessor>("proc");

  auto connection = std::make_shared<minifi::Connection>(ff_repo, content_repo, swap_manager, "conn", minifi::utils::IdGenerator::getIdGenerator()->generate());
  connection->setSwapThreshold(50);
  connection->addRelationship(OutputProcessor::Success);
  connection->setSourceUUID(processor->getUUID());
  processor->addConnection(connection.get());

  auto processor_node = std::make_shared<core::ProcessorNode>(processor.get());
  auto context = std::make_shared<core::ProcessContext>(processor_node, nullptr, prov_repo, ff_repo, content_repo);
  auto session_factory = std::make_shared<core::ProcessSessionFactory>(context);

  for (size_t i = 0; i < 200; ++i) {
    processor->onTrigger(context, session_factory);
  }

  REQUIRE(connection->getQueueSize() == processor->flow_files_.size());
  minifi::utils::FlowFileQueue& queue = utils::ConnectionTestAccessor::get_queue_(*connection);
  // below max threshold live flow files
  REQUIRE(utils::FlowFileQueueTestAccessor::get_queue_(queue).size() <= 75);
  REQUIRE(queue.size() == 200);

  std::set<std::shared_ptr<core::FlowFile>> expired;
  for (size_t i = 0; i < 200; ++i) {
    std::shared_ptr<core::FlowFile> ff;
    bool got_non_null_flow_file = minifi::utils::verifyEventHappenedInPollTime(std::chrono::seconds{5}, [&] {
      ff = connection->poll(expired);
      return static_cast<bool>(ff);
    });
    REQUIRE(got_non_null_flow_file);
    REQUIRE(ff->getAttribute("index") == std::to_string(i));
    REQUIRE(ff->getResourceClaim()->getContentFullPath() == processor->flow_files_[i]->getResourceClaim()->getContentFullPath());
  }

  REQUIRE(queue.empty());
}

}  // namespace org::apache::nifi::minifi::test
