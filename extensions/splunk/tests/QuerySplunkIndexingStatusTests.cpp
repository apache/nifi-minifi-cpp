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

#include "QuerySplunkIndexingStatus.h"
#include "MockSplunkHEC.h"
#include "SplunkAttributes.h"
#include "TestBase.h"
#include "Catch.h"
#include "processors/UpdateAttribute.h"
#include "ReadFromFlowFileTestProcessor.h"
#include "WriteToFlowFileTestProcessor.h"

using QuerySplunkIndexingStatus = org::apache::nifi::minifi::extensions::splunk::QuerySplunkIndexingStatus;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;
using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;
using namespace std::literals::chrono_literals;

TEST_CASE("QuerySplunkIndexingStatus tests", "[querysplunkindexingstatus]") {
  MockSplunkHEC mock_splunk_hec("10132");

  TestController test_controller;
  auto plan = test_controller.createPlan();
  auto write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  auto update_attribute = std::dynamic_pointer_cast<UpdateAttribute>(plan->addProcessor("UpdateAttribute", "update_attribute"));
  auto query_splunk_indexing_status = std::dynamic_pointer_cast<QuerySplunkIndexingStatus>(plan->addProcessor("QuerySplunkIndexingStatus", "query_splunk_indexing_status"));
  auto read_from_acknowledged = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_acknowledged"));
  auto read_from_undetermined = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_undetermined"));
  auto read_from_unacknowledged = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_unacknowledged"));
  auto read_from_failure = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure"));

  plan->addConnection(write_to_flow_file, WriteToFlowFileTestProcessor::Success, update_attribute);
  plan->addConnection(update_attribute, UpdateAttribute ::Success, query_splunk_indexing_status);
  plan->addConnection(query_splunk_indexing_status, QuerySplunkIndexingStatus::Acknowledged, read_from_acknowledged);
  plan->addConnection(query_splunk_indexing_status, QuerySplunkIndexingStatus::Undetermined, read_from_undetermined);
  plan->addConnection(query_splunk_indexing_status, QuerySplunkIndexingStatus::Unacknowledged, read_from_unacknowledged);
  plan->addConnection(query_splunk_indexing_status, QuerySplunkIndexingStatus::Failure, read_from_failure);

  read_from_acknowledged->setAutoTerminatedRelationships(std::array{core::Relationship{ReadFromFlowFileTestProcessor::Success}});
  read_from_undetermined->setAutoTerminatedRelationships(std::array{core::Relationship{ReadFromFlowFileTestProcessor::Success}});
  read_from_unacknowledged->setAutoTerminatedRelationships(std::array{core::Relationship{ReadFromFlowFileTestProcessor::Success}});
  read_from_failure->setAutoTerminatedRelationships(std::array{core::Relationship{ReadFromFlowFileTestProcessor::Success}});

  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Hostname, "localhost");
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Port, mock_splunk_hec.getPort());
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Token, MockSplunkHEC::TOKEN);
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::SplunkRequestChannel, "a12254b4-f481-435d-896d-3b6033eabe58");

  auto response_timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME, response_timestamp);

  write_to_flow_file->setContent("foobar");

  SECTION("Querying indexed id") {
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, std::to_string(MockSplunkHEC::indexed_events[0]));
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 1);
  }

  SECTION("Querying not indexed id") {
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "100");
    query_splunk_indexing_status->setPenalizationPeriod(50ms);
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);  // result penalized
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);

    write_to_flow_file->setContent("");
    plan->reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100ms));
    test_controller.runSession(plan);

    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 1);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("Querying not indexed old id") {
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "100");
    response_timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - 2h).time_since_epoch()).count());

    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME, response_timestamp);
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 1);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("Multiple inputs with same id") {
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, std::to_string(MockSplunkHEC::indexed_events[0]));
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME, response_timestamp);
    for (size_t i = 0; i < 4; ++i) {
      plan->runProcessor(write_to_flow_file);
      plan->runProcessor(update_attribute);
    }
    plan->runProcessor(query_splunk_indexing_status);
    plan->runProcessor(read_from_failure);
    plan->runProcessor(read_from_undetermined);
    plan->runProcessor(read_from_unacknowledged);
    plan->runProcessor(read_from_acknowledged);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 4);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("MaxQuerySize can limit the number of queries") {
    plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::MaxQuerySize, "5");
    for (size_t i = 0; i < 10; ++i) {
      plan->runProcessor(write_to_flow_file);
      plan->runProcessor(update_attribute);
    }
    plan->runProcessor(query_splunk_indexing_status);
    CHECK(plan->getNumFlowFileProducedByProcessor(query_splunk_indexing_status) == 5);
  }

  SECTION("Input flow file has no attributes") {
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 1);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("Invalid index") {
    plan->setDynamicProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "foo");
    REQUIRE_THROWS(test_controller.runSession(plan));
  }
}

