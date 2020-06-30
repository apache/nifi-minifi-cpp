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

#include <string>

#include <catch.hpp>
#include "core/ProcessSession.h"
#include "../TestBase.h"

namespace {

class DummyProcessor : public core::Processor {
  using core::Processor::Processor;
};

REGISTER_RESOURCE(DummyProcessor, "A processor that does nothing.")

class Fixture {
 public:
  Fixture();
  core::ProcessSession &processSession() { return *process_session_; }

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

Fixture::Fixture() {
  test_plan_ = test_controller_.createPlan();
  dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  test_plan_->runNextProcessor();  // set the dummy processor as current
  context_ = test_plan_->getCurrentContext();
  process_session_ = utils::make_unique<core::ProcessSession>(context_);
}

const core::Relationship Success{"success", "everything is fine"};
const core::Relationship Failure{"failure", "something has gone awry"};

}  // namespace

TEST_CASE("ProcessSession::existsFlowFileInRelationship works", "[existsFlowFileInRelationship]") {
  Fixture fixture;
  core::ProcessSession &process_session = fixture.processSession();

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
