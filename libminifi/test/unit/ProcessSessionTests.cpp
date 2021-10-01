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

#include <memory>
#include <string>

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "../TestBase.h"
#include "ContentRepositoryDependentTests.h"
#include "Processor.h"

namespace {

class DummyProcessor : public minifi::core::Processor {
  using minifi::core::Processor::Processor;
};

REGISTER_RESOURCE(DummyProcessor, "A processor that does nothing.");

class Fixture {
 public:
  minifi::core::ProcessSession &processSession() { return *process_session_; }

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan();
  std::shared_ptr<minifi::core::Processor> dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<minifi::core::ProcessContext> context_ = [this] { test_plan_->runNextProcessor(); return test_plan_->getCurrentContext(); }();
  std::unique_ptr<minifi::core::ProcessSession> process_session_ = std::make_unique<core::ProcessSession>(context_);
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
  ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<minifi::core::repository::VolatileContentRepository>());
  ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<minifi::core::repository::FileSystemRepository>());

  ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<minifi::core::repository::VolatileContentRepository>());
  ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<minifi::core::repository::FileSystemRepository>());
}
