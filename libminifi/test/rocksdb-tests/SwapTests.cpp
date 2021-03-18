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

#include "core/RepositoryFactory.h"
#include "core/repository/VolatileContentRepository.h"
#include "FlowFileRepository.h"
#include "../TestBase.h"
#include "../Utils.h"
#include "StreamPipe.h"
#include "IntegrationTestUtils.h"

class OutputProcessor : public core::Processor {
 public:
  using core::Processor::Processor;

  static const core::Relationship Success;

  using core::Processor::onTrigger;

  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) override {
    int id = next_id_++;
    auto ff = session->create();
    ff->addAttribute("index", std::to_string(id));
    auto buffer = std::make_shared<minifi::io::BufferStream>(std::to_string(id));
    minifi::OutputStreamPipe input{buffer};
    session->write(ff, &input);
    session->transfer(ff, Success);
    flow_files_.push_back(ff);
  }

  std::vector<std::shared_ptr<core::FlowFile>> flow_files_;
  int next_id_{0};
};

const core::Relationship OutputProcessor::Success{"success", ""};

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
  LogTestController::getInstance().setTrace<core::repository::VolatileRepository<minifi::ResourceClaim::Path>>();

  char format[] = "/var/tmp/test.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, utils::file::FileUtils::concat_path(dir, "content_repository"));
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> ff_repo = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
  std::shared_ptr<minifi::SwapManager> swap_manager{ff_repo, ff_repo->castToSwapManager()};
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  ff_repo->initialize(config);
  content_repo->initialize(config);

  ff_repo->loadComponent(content_repo);
  ff_repo->start();

  auto processor = std::make_shared<OutputProcessor>("proc");

  auto connection = std::make_shared<minifi::Connection>(ff_repo, content_repo, swap_manager, "conn", utils::IdGenerator::getIdGenerator()->generate());
  connection->setSwapThreshold(50);
  connection->addRelationship(OutputProcessor::Success);
  connection->setSourceUUID(processor->getUUID());
  processor->addConnection(connection);

  auto processor_node = std::make_shared<core::ProcessorNode>(processor);
  auto context = std::make_shared<core::ProcessContext>(processor_node, nullptr, prov_repo, ff_repo, content_repo);
  auto session_factory = std::make_shared<core::ProcessSessionFactory>(context);

  for (size_t i = 0; i < 200; ++i) {
    processor->onTrigger(context, session_factory);
  }

  REQUIRE(connection->getQueueSize() == processor->flow_files_.size());
  utils::FlowFileQueue& queue = ConnectionTestAccessor::get_queue_(*connection);
  // below max threshold live flow files
  REQUIRE(FlowFileQueueTestAccessor::get_queue_(queue).size() <= 75);
  REQUIRE(queue.size() == 200);

  std::set<std::shared_ptr<core::FlowFile>> expired;
  for (size_t i = 0; i < 200; ++i) {
    std::shared_ptr<core::FlowFile> ff;
    minifi::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, [&] {
      ff = connection->poll(expired);
      return static_cast<bool>(ff);
    });
    REQUIRE(ff->getAttribute("index") == std::to_string(i));
    REQUIRE(ff->getResourceClaim()->getContentFullPath() == processor->flow_files_[i]->getResourceClaim()->getContentFullPath());
  }

  REQUIRE(queue.empty());
}
