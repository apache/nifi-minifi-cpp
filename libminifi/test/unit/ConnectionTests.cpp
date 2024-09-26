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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"

TEST_CASE("Connection::poll() works correctly", "[poll]") {
  const auto flow_repo = std::make_shared<TestRepository>();
  const auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());

  const auto id_generator = utils::IdGenerator::getIdGenerator();
  utils::Identifier connection_id = id_generator->generate();
  utils::Identifier src_id = id_generator->generate();
  utils::Identifier dest_id = id_generator->generate();

  const auto connection = std::make_shared<minifi::ConnectionImpl>(flow_repo, content_repo, "test_connection", connection_id, src_id, dest_id);
  std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;

  SECTION("when called on an empty Connection, poll() returns nullptr") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single flow file, poll() returns the flow file") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto flow_file = std::make_shared<core::FlowFileImpl>();
    connection->put(flow_file);
    REQUIRE(flow_file == connection->poll(expired_flow_files));
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single penalized flow file, poll() returns nullptr") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto flow_file = std::make_shared<core::FlowFileImpl>();
    flow_file->penalize(std::chrono::seconds{10});
    connection->put(flow_file);
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }

  SECTION("when called on a connection with a single expired flow file, poll() returns nullptr and returns the expired flow file in the out parameter") {
    const auto flow_file = std::make_shared<core::FlowFileImpl>();
    connection->setFlowExpirationDuration(1ms);
    connection->put(flow_file);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    REQUIRE(nullptr == connection->poll(expired_flow_files));
    REQUIRE(std::set<std::shared_ptr<core::FlowFile>>{flow_file} == expired_flow_files);
  }

  SECTION("when there is a non-penalized flow file followed by a penalized flow file, poll() returns the non-penalized flow file") {
    SECTION("without expiration duration") {}
    SECTION("with expiration duration") { connection->setFlowExpirationDuration(1s); }

    const auto penalized_flow_file = std::make_shared<core::FlowFileImpl>();
    penalized_flow_file->penalize(std::chrono::seconds{10});
    connection->put(penalized_flow_file);

    const auto flow_file = std::make_shared<core::FlowFileImpl>();
    connection->put(flow_file);

    REQUIRE(flow_file == connection->poll(expired_flow_files));
    REQUIRE(nullptr == connection->poll(expired_flow_files));
  }
}

TEST_CASE("Connection backpressure tests", "[Connection]") {
  const auto flow_repo = std::make_shared<TestRepository>();
  const auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());

  const auto id_generator = utils::IdGenerator::getIdGenerator();
  const auto connection = std::make_shared<minifi::ConnectionImpl>(flow_repo, content_repo, "test_connection", id_generator->generate(), id_generator->generate(), id_generator->generate());

  CHECK(connection->getBackpressureThresholdDataSize() == minifi::Connection::DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE);
  CHECK(connection->getBackpressureThresholdCount() == minifi::Connection::DEFAULT_BACKPRESSURE_THRESHOLD_COUNT);

  SECTION("The number of flowfiles can be limited") {
    connection->setBackpressureThresholdCount(2);
    CHECK_FALSE(connection->backpressureThresholdReached());
    connection->put(std::make_shared<core::FlowFileImpl>());
    CHECK_FALSE(connection->backpressureThresholdReached());
    connection->put(std::make_shared<core::FlowFileImpl>());
    CHECK(connection->backpressureThresholdReached());
    connection->setBackpressureThresholdCount(0);
    CHECK_FALSE(connection->backpressureThresholdReached());
  }
  SECTION("The size of the data can be limited") {
    connection->setBackpressureThresholdDataSize(3_KB);
    CHECK_FALSE(connection->backpressureThresholdReached());
    {
      auto flow_file = std::make_shared<core::FlowFileImpl>();
      flow_file->setSize(2_KB);
      connection->put(flow_file);
    }
    CHECK_FALSE(connection->backpressureThresholdReached());
    {
      auto flow_file = std::make_shared<core::FlowFileImpl>();
      flow_file->setSize(2_KB);
      connection->put(flow_file);
    }
    CHECK(connection->backpressureThresholdReached());
    connection->setBackpressureThresholdDataSize(0);
    CHECK_FALSE(connection->backpressureThresholdReached());
  }
}
