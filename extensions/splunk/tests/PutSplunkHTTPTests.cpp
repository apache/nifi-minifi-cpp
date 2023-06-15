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
#include "Catch.h"
#include "ReadFromFlowFileTestProcessor.h"
#include "WriteToFlowFileTestProcessor.h"
#include "core/Relationship.h"
#include "processors/UpdateAttribute.h"
#include "MockSplunkHEC.h"

using PutSplunkHTTP = org::apache::nifi::minifi::extensions::splunk::PutSplunkHTTP;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;
using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;


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

  read_from_success->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  read_from_failure->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});

  plan->setProperty(put_splunk_http, PutSplunkHTTP::Hostname, "localhost");
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Port, mock_splunk_hec.getPort());
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Token, MockSplunkHEC::TOKEN);
  plan->setProperty(put_splunk_http, PutSplunkHTTP::SplunkRequestChannel, "a12254b4-f481-435d-896d-3b6033eabe58");

  write_to_flow_file->setContent("foobar");

  SECTION("Happy path") {
    mock_splunk_hec.setAssertions([](const struct mg_request_info *request_info) {
      CHECK(request_info->query_string == nullptr);
    });
    test_controller.runSession(plan);
    CHECK(read_from_failure->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success->numberOfFlowFilesRead() == 1);
    CHECK(read_from_success->readFlowFileWithContent("foobar"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_STATUS_CODE, "200"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_CODE, "0"));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_RESPONSE_TIME));
    CHECK(read_from_success->readFlowFileWithAttribute(org::apache::nifi::minifi::extensions::splunk::SPLUNK_ACK_ID));
  }

  SECTION("Happy path with query arguments") {
    plan->setProperty(put_splunk_http, PutSplunkHTTP::Source, "foo");
    plan->setProperty(put_splunk_http, PutSplunkHTTP::SourceType, "bar");
    plan->setProperty(put_splunk_http, PutSplunkHTTP::Host, "baz");
    plan->setProperty(put_splunk_http, PutSplunkHTTP::Index, "qux");
    mock_splunk_hec.setAssertions([](const struct mg_request_info *request_info) {
      std::string query_string = request_info->query_string;
      CHECK(!query_string.empty());
      CHECK(query_string.find("source=foo") != std::string::npos);
      CHECK(query_string.find("sourcetype=bar") != std::string::npos);
      CHECK(query_string.find("host=baz") != std::string::npos);
      CHECK(query_string.find("index=qux") != std::string::npos);
    });
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
    plan->setProperty(put_splunk_http, PutSplunkHTTP::Token, invalid_token);
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

namespace {
struct ContentTypeValidator {
  explicit ContentTypeValidator(std::string required_content_type) : required_content_type_(std::move(required_content_type)) {
  }
  void operator() (const struct mg_request_info* req_info) const {
    if (!required_content_type_)
      return;
    auto content_type_header = std::find_if(std::begin(req_info->http_headers),
                                            std::end(req_info->http_headers),
                                            [](auto header) -> bool {return strcmp(header.name, "Content-Type") == 0;});
    REQUIRE(content_type_header != std::end(req_info->http_headers));
    CHECK(strcmp(content_type_header->value, required_content_type_.value().c_str()) == 0);
  }
  std::optional<std::string> required_content_type_;
};
}  // namespace

TEST_CASE("PutSplunkHTTP content type tests", "[putsplunkhttpcontenttype]") {
  MockSplunkHEC mock_splunk_hec("10131");

  TestController test_controller;
  auto plan = test_controller.createPlan();
  auto write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  auto update_attribute = std::dynamic_pointer_cast<UpdateAttribute>(plan->addProcessor("UpdateAttribute", "update_attribute"));
  auto put_splunk_http = std::dynamic_pointer_cast<PutSplunkHTTP>(plan->addProcessor("PutSplunkHTTP", "put_splunk_http"));

  plan->addConnection(write_to_flow_file, WriteToFlowFileTestProcessor::Success, update_attribute);
  plan->addConnection(update_attribute, UpdateAttribute::Success, put_splunk_http);
  put_splunk_http->setAutoTerminatedRelationships(std::array<core::Relationship, 2>{PutSplunkHTTP::Success, PutSplunkHTTP::Failure});

  plan->setProperty(put_splunk_http, PutSplunkHTTP::Hostname, "localhost");
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Port, mock_splunk_hec.getPort());
  plan->setProperty(put_splunk_http, PutSplunkHTTP::Token, MockSplunkHEC::TOKEN);
  plan->setProperty(put_splunk_http, PutSplunkHTTP::SplunkRequestChannel, "a12254b4-f481-435d-896d-3b6033eabe58");

  write_to_flow_file->setContent("foobar");

  SECTION("Content Type without Property or Attribute") {
    mock_splunk_hec.setAssertions(ContentTypeValidator("application/x-www-form-urlencoded"));
    test_controller.runSession(plan);
  }

  SECTION("Content Type with Processor Property") {
    plan->setProperty(put_splunk_http, PutSplunkHTTP::ContentType, "from_property");
    mock_splunk_hec.setAssertions(ContentTypeValidator("from_property"));
    test_controller.runSession(plan);
  }

  SECTION("Content Type with FlowFile Attribute") {
    plan->setDynamicProperty(update_attribute, "mime.type", "from_attribute");
    mock_splunk_hec.setAssertions(ContentTypeValidator("from_attribute"));
    test_controller.runSession(plan);
  }

  SECTION("Content Type with Property and Attribute") {
    plan->setDynamicProperty(update_attribute, "mime.type", "from_attribute");
    plan->setProperty(put_splunk_http, PutSplunkHTTP::ContentType, "from_property");
    mock_splunk_hec.setAssertions(ContentTypeValidator("from_property"));
    test_controller.runSession(plan);
  }
}

