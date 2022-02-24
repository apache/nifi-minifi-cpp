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

#include "../TestBase.h"
#include "../Catch.h"
#include "FlowFileRepository.h"
#include "utils/IntegrationTestUtils.h"
#include "../Path.h"
#include "repository/VolatileContentRepository.h"
#include "FlowFileRecord.h"

using utils::Path;
using core::repository::FlowFileRepository;

class FFRepoFixture : public TestController {
 public:
  FFRepoFixture() {
    LogTestController::getInstance().setDebug<minifi::FlowFileRecord>();
    LogTestController::getInstance().setDebug<minifi::Connection>();
    LogTestController::getInstance().setTrace<FlowFileRepository>();
    home_ = utils::Path{createTempDirectory()};
    repo_dir_ = home_ / "flowfile_repo";
    checkpoint_dir_ = home_ / "checkpoint_dir";
    config_ = std::make_shared<minifi::Configure>();
    config_->setHome(home_.str());
    container_ = std::make_unique<minifi::Connection>(nullptr, nullptr, "container");
    content_repo_ = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo_->initialize(config_);
  }

  static void putFlowFile(const std::shared_ptr<minifi::FlowFileRecord>& flowfile, const std::shared_ptr<core::repository::FlowFileRepository>& repo) {
    minifi::io::BufferStream buffer;
    flowfile->Serialize(buffer);
    const auto buf = buffer.getBuffer().as_span<const uint8_t>();
    REQUIRE(repo->Put(flowfile->getUUIDStr(), buf.data(), buf.size()));
  }

  template<typename Fn>
  void runWithNewRepository(Fn&& fn) {
    auto repository = std::make_shared<FlowFileRepository>("ff", checkpoint_dir_.str(), repo_dir_.str());
    repository->initialize(config_);
    std::map<std::string, core::Connectable*> container_map;
    container_map[container_->getUUIDStr()] = container_.get();
    repository->setContainers(container_map);
    repository->loadComponent(content_repo_);
    repository->start();
    std::forward<Fn>(fn)(repository);
    repository->stop();
  }

 protected:
  std::unique_ptr<minifi::Connection> container_;
  Path home_;
  Path repo_dir_;
  Path checkpoint_dir_;
  std::shared_ptr<minifi::Configure> config_;
  std::shared_ptr<core::repository::VolatileContentRepository> content_repo_;
};

TEST_CASE_METHOD(FFRepoFixture, "FlowFileRepository creates checkpoint and loads flowfiles") {
  SECTION("Without encryption") {
    // pass
  }
  SECTION("With encryption") {
    utils::file::FileUtils::create_dir((home_ / "conf").str());
    std::ofstream{(home_ / "conf" / "bootstrap.conf").str()}
      << static_cast<const char*>(FlowFileRepository::ENCRYPTION_KEY_NAME) << "="
      << "805D7B95EF44DC27C87FFBC4DFDE376DAE604D55DB2C5496DEEF5236362DE62E"
      << "\n";
  }


  runWithNewRepository([&] (const std::shared_ptr<core::repository::FlowFileRepository>& repo) {
    auto flowfile = std::make_shared<minifi::FlowFileRecord>();
    flowfile->setAttribute("my little pony", "my horse is amazing");
    flowfile->setConnection(container_.get());
    putFlowFile(flowfile, repo);
  });

  REQUIRE(container_->isEmpty());

  runWithNewRepository([&] (const std::shared_ptr<core::repository::FlowFileRepository>& /*repo*/) {
    // wait for the flowfiles to be loaded from the checkpoint
    bool success = utils::verifyEventHappenedInPollTime(std::chrono::seconds{5}, [&] {
      return !container_->isEmpty();
    });
    REQUIRE(success);
    REQUIRE(utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds{5},
        "Successfully opened checkpoint database at '" + checkpoint_dir_.str() + "'"));
    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto flowfile = container_->poll(expired);
    REQUIRE(expired.empty());
    REQUIRE(flowfile);
    REQUIRE(flowfile->getAttribute("my little pony") == "my horse is amazing");
  });
}
