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

#include "PutSplunkHTTP.h"
#include "SplunkAttributes.h"
#include "TestBase.h"
#include "ReadFromFlowFileTestProcessor.h"
#include "WriteToFlowFileTestProcessor.h"
#include "MockSplunkHEC.h"

using PutSplunkHTTP = org::apache::nifi::minifi::extensions::splunk::PutSplunkHTTP;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;


TEST_CASE("PutSplunkHTTP tests", "[putsplunkhttp]") {
  MockSplunkHEC mock_splunk_hec("10133");

  TestController test_controller;
  auto plan = test_controller.createPlan();
  auto write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  auto put_splunk_http = std::dynamic_pointer_cast<PutSplunkHTTP>(plan->addProcessor("PutSplunkHTTP", "put_splunk_http"));
  auto read_from_success = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_success"));
  auto read_from_failure = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure"));

  plan->addConnection(write_to_flow_file, WriteToFlowFileTestProcessor::Success, put_splunk_http);
  plan->addConnection(put_splunk_http, PutSplunkHTTP::Success, read_from_success);
  plan->addConnection(put_splunk_http, PutSplunkHTTP::Failure, read_from_failure);

  read_from_success->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});
  read_from_failure->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});

  plan->setProperty(put_splunk_http, PutSplunkHTTP::Hostname.getName(), "localhost");
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Port.getName(), mock_splunk_hec.getPort());
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Token.getName(), MockSplunkHEC::TOKEN);
  plan->setProperty(put_splunk_http, PutSplunkHTTP::SplunkRequestChannel.getName(), "a12254b4-f481-435d-896d-3b6033eabe58");

  write_to_flow_file->setContent("foobar");

  SECTION("Happy path") {
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success->numberOfFlowFilesRead() == 1);
    CHECK(read_from_success->readFlowFileWithContent("foobar"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_STATUS_CODE, "200"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_CODE, "0"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID));
  }

  SECTION("Invalid Token") {
    constexpr const char* invalid_token = "Splunk 00000000-0000-0000-0000-000000000000";
    plan->setProperty(put_splunk_http, PutSplunkHTTP::Token.getName(), invalid_token);
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 1);
    CHECK(read_from_success->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure->readFlowFileWithContent("foobar"));
    CHECK(read_from_failure->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_STATUS_CODE, "403"));
    CHECK(read_from_failure->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_CODE, "4"));
    CHECK(read_from_failure->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME));
    CHECK_FALSE(read_from_failure->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID));
  }
}

