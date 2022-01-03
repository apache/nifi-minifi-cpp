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

#include <chrono>

#include "QuerySplunkIndexingStatus.h"
#include "MockSplunkHEC.h"
#include "SplunkAttributes.h"
#include "TestBase.h"
#include "processors/UpdateAttribute.h"
#include "ReadFromFlowFileTestProcessor.h"
#include "WriteToFlowFileTestProcessor.h"
#include "utils/TimeUtil.h"

using QuerySplunkIndexingStatus = org::apache::nifi::minifi::extensions::splunk::QuerySplunkIndexingStatus;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;
using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;
using namespace std::chrono_literals;  // NOLINT(build/namespaces)

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

  read_from_acknowledged->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});
  read_from_undetermined->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});
  read_from_unacknowledged->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});
  read_from_failure->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});

  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Hostname.getName(), "localhost");
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Port.getName(), mock_splunk_hec.getPort());
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::Token.getName(), MockSplunkHEC::TOKEN);
  plan->setProperty(query_splunk_indexing_status, QuerySplunkIndexingStatus::SplunkRequestChannel.getName(), "a12254b4-f481-435d-896d-3b6033eabe58");

  auto response_timestamp = std::to_string(utils::timeutils::getTimestamp<std::chrono::milliseconds>(std::chrono::system_clock::now()));
  plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME, response_timestamp, true);

  write_to_flow_file->setContent("foobar");

  SECTION("Querying indexed id") {
    plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, std::to_string(MockSplunkHEC::indexed_events[0]), true);
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 1);
  }

  SECTION("Querying not indexed id") {
    plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "100", true);
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
    plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "100", true);
    response_timestamp = std::to_string(utils::timeutils::getTimestamp<std::chrono::milliseconds>(std::chrono::system_clock::now() - 2h));
    plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME, response_timestamp, true);
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 1);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("Input flow file has no attributes") {
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 1);
    CHECK(read_from_undetermined->numberOfFlowFilesRead() == 0);
    CHECK(read_from_unacknowledged->numberOfFlowFilesRead() == 0);
    CHECK(read_from_acknowledged->numberOfFlowFilesRead() == 0);
  }

  SECTION("Invalid index") {
    plan->setProperty(update_attribute, org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID, "foo", true);
    REQUIRE_THROWS(test_controller.runSession(plan));
  }
}

