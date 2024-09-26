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
#include <memory>
#include <string>

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ContentRepositoryDependentTests.h"
#include "core/Processor.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "unit/TestUtils.h"
#include "core/repository/FileSystemRepository.h"

namespace {

class Fixture {
 public:
  explicit Fixture(TestController::PlanConfig config = {}): plan_config_(std::move(config)) {}

  minifi::core::ProcessSession &processSession() { return *process_session_; }

 private:
  TestController test_controller_;
  TestController::PlanConfig plan_config_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan(plan_config_);
  std::shared_ptr<minifi::core::Processor> dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<minifi::core::ProcessContext> context_ = [this] { test_plan_->runNextProcessor(); return test_plan_->getCurrentContext(); }();
  std::unique_ptr<minifi::core::ProcessSession> process_session_ = std::make_unique<core::ProcessSessionImpl>(context_);
};

const minifi::core::Relationship Success{"success", "everything is fine"};
const minifi::core::Relationship Failure{"failure", "something has gone awry"};

}  // namespace

TEST_CASE("ProcessSession::existsFlowFileInRelationship works", "[existsFlowFileInRelationship]") {
  Fixture fixture;
  minifi::core::ProcessSession &process_session = fixture.processSession();

  REQUIRE_FALSE(process_session.existsFlowFileInRelationship(Failure));
  REQUIRE_FALSE(process_session.existsFlowFileInRelationship(Success));

  const auto flow_file_1 = process_session.create();
  process_session.transfer(flow_file_1, Failure);

  REQUIRE(process_session.existsFlowFileInRelationship(Failure));
  REQUIRE_FALSE(process_session.existsFlowFileInRelationship(Success));

  const auto flow_file_2 = process_session.create();
  process_session.transfer(flow_file_2, Success);

  REQUIRE(process_session.existsFlowFileInRelationship(Failure));
  REQUIRE(process_session.existsFlowFileInRelationship(Success));
}

TEST_CASE("ProcessSession::rollback penalizes affected flowfiles", "[rollback]") {
  Fixture fixture;
  minifi::core::ProcessSession &process_session = fixture.processSession();

  const auto flow_file_1 = process_session.create();
  const auto flow_file_2 = process_session.create();
  const auto flow_file_3 = process_session.create();
  process_session.transfer(flow_file_1, Success);
  process_session.transfer(flow_file_2, Success);
  process_session.transfer(flow_file_3, Success);
  process_session.commit();

  auto next_flow_file_to_be_processed = process_session.get();
  REQUIRE(next_flow_file_to_be_processed == flow_file_1);
  next_flow_file_to_be_processed = process_session.get();
  REQUIRE(next_flow_file_to_be_processed == flow_file_2);
  REQUIRE_FALSE(flow_file_1->isPenalized());
  REQUIRE_FALSE(flow_file_2->isPenalized());
  REQUIRE_FALSE(flow_file_3->isPenalized());

  process_session.rollback();
  REQUIRE(flow_file_1->isPenalized());
  REQUIRE(flow_file_2->isPenalized());
  REQUIRE_FALSE(flow_file_3->isPenalized());
  next_flow_file_to_be_processed = process_session.get();
  REQUIRE(next_flow_file_to_be_processed == flow_file_3);
}

TEST_CASE("ProcessSession::read reads the flowfile from offset to size", "[readoffsetsize]") {
  ContentRepositoryDependentTests::testReadOnSmallerClonedFlowFiles(std::make_shared<minifi::core::repository::VolatileContentRepository>());
  ContentRepositoryDependentTests::testReadOnSmallerClonedFlowFiles(std::make_shared<minifi::core::repository::FileSystemRepository>());
}

TEST_CASE("ProcessSession::append should append to the flowfile and set its size correctly" "[appendsetsize]") {
  SECTION("Unmanaged") {
    SECTION("VolatileContentRepository") {
      ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<minifi::core::repository::VolatileContentRepository>());
    }
    SECTION("FileSystemRepository") {
      ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<minifi::core::repository::FileSystemRepository>());
    }
  }

  SECTION("Managed") {
    SECTION("VolatileContentRepository") {
      ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<minifi::core::repository::VolatileContentRepository>());
    }
    SECTION("FileSystemRepository") {
      ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<minifi::core::repository::FileSystemRepository>());
    }
  }
}

TEST_CASE("ProcessSession::read can read zero length flowfiles without crash", "[zerolengthread]") {
  ContentRepositoryDependentTests::testReadFromZeroLengthFlowFile(std::make_shared<core::repository::VolatileContentRepository>());
  ContentRepositoryDependentTests::testReadFromZeroLengthFlowFile(std::make_shared<core::repository::FileSystemRepository>());
}

struct VolatileFlowFileRepositoryTestAccessor {
  METHOD_ACCESSOR(flush);
};

class TestVolatileFlowFileRepository : public core::repository::VolatileFlowFileRepository {
 public:
  explicit TestVolatileFlowFileRepository(const std::string& name) : core::repository::VolatileFlowFileRepository(name) {}

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override {
    auto flush_on_exit = gsl::finally([&] {VolatileFlowFileRepositoryTestAccessor::call_flush(*this);});
    return VolatileFlowFileRepository::MultiPut(data);
  }
};

TEST_CASE("ProcessSession::commit avoids dangling ResourceClaims when using VolatileFlowFileRepository", "[incrementbefore]") {
  TempDirectory tmp_dir;
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(tmp_dir.getPath());
  configuration->set(minifi::Configure::nifi_volatile_repository_options_flowfile_max_count, "2");
  auto ff_repo = std::make_shared<TestVolatileFlowFileRepository>("flowfile");
  Fixture fixture({
    .configuration = std::move(configuration),
    .flow_file_repo = ff_repo
  });
  auto& session = fixture.processSession();

  const auto flow_file_1 = session.create();
  const auto flow_file_2 = session.create();
  const auto flow_file_3 = session.create();
  session.transfer(flow_file_1, Success);
  session.transfer(flow_file_2, Success);
  session.transfer(flow_file_3, Success);
  session.commit();

  // flow_files are owned by the shared_ptr on the stack and the ff_repo
  // but the first one has been evicted from the ff_repo
  REQUIRE(flow_file_1->getResourceClaim()->getFlowFileRecordOwnedCount() == 1);
  REQUIRE(flow_file_2->getResourceClaim()->getFlowFileRecordOwnedCount() == 2);
  REQUIRE(flow_file_3->getResourceClaim()->getFlowFileRecordOwnedCount() == 2);

  REQUIRE(flow_file_1->getResourceClaim()->exists());
  REQUIRE(flow_file_2->getResourceClaim()->exists());
  REQUIRE(flow_file_3->getResourceClaim()->exists());
}
