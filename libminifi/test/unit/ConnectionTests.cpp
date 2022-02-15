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
#include "Connection.h"

#include "../TestBase.h"
#include "../Catch.h"
#include "ProvenanceTestHelper.h"

TEST_CASE("Connection::poll() works correctly", "[poll]") {
  const auto flow_repo = std::make_shared<TestRepository>();
  const auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(std::make_shared<minifi::Configure>());

  const auto id_generator = utils::IdGenerator::getIdGenerator();
  utils::Identifier connection_id = id_generator->generate();
  utils::Identifier src_id = id_generator->generate();
  utils::Identifier dest_id = id_generator->generate();

  const auto connection = std::make_shared<minifi::Connection>(flow_repo, content_repo, "test_connection", connection_id, src_id, dest_id);
  std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;

  SECTION("when called on an empty Connection, poll() returns nullptr") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single flow file, poll() returns the flow file") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto flow_file = std::make_shared<core::FlowFile>();
    connection->put(flow_file);
    REQUIRE(flow_file == connection->poll(expired_flow_files));
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single penalized flow file, poll() returns nullptr") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto flow_file = std::make_shared<core::FlowFile>();
    flow_file->penalize(std::chrono::seconds{10});
    connection->put(flow_file);
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single expired flow file, poll() returns nullptr and returns the expired flow file in the out parameter") {
    const auto flow_file = std::make_shared<core::FlowFile>();
    connection->setFlowExpirationDuration(1ms);
    connection->put(flow_file);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    REQUIRE(nullptr == connection->poll(expired_flow_files));
    REQUIRE(std::set<std::shared_ptr<core::FlowFile>>{flow_file} == expired_flow_files);
  }

  SECTION("when there is a non-penalized flow file followed by a penalized flow file, poll() returns the non-penalized flow file") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto penalized_flow_file = std::make_shared<core::FlowFile>();
    penalized_flow_file->penalize(std::chrono::seconds{10});
    connection->put(penalized_flow_file);

    const auto flow_file = std::make_shared<core::FlowFile>();
    connection->put(flow_file);

    REQUIRE(flow_file == connection->poll(expired_flow_files));
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }
}
