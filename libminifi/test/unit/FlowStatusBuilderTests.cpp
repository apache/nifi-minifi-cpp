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
#include <unordered_set>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "c2/FlowStatusBuilder.h"
#include "unit/DummyProcessor.h"
#include "core/BulletinStore.h"
#include "properties/Configure.h"
#include "unit/ProcessorUtils.h"
#include "Connection.h"
#include "core/FlowFile.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

namespace org::apache::nifi::minifi::test {

TEST_CASE("Parse invalid flow status query string", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid query string"), "Invalid query string: invalid query string");
}

TEST_CASE("Parse invalid flow status query type", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid_type:TaiFile:health"), "Invalid query type: invalid_type");
}

TEST_CASE("Parse two part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:health,stats");
  CHECK(request.query_type == c2::FlowStatusQueryType::processor);
  CHECK(request.identifier.empty());
  CHECK(request.options == std::unordered_set<c2::FlowStatusQueryOption>{c2::FlowStatusQueryOption::health, c2::FlowStatusQueryOption::stats});
}

TEST_CASE("Parse three part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:TailFile:health");
  CHECK(request.query_type == c2::FlowStatusQueryType::processor);
  CHECK(request.identifier == "TailFile");
  CHECK(request.options == std::unordered_set<c2::FlowStatusQueryOption>{c2::FlowStatusQueryOption::health});
}

TEST_CASE("Build empty flow status", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  auto status = flow_status_builder.buildFlowStatus({});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:4d7fa7e6-2459-46dd-b2ba-61517239edf5:health"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["bulletinList"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"]["runStatus"] == "Stopped");
  CHECK_FALSE(status["processorStatusList"].GetArray()[0]["processorHealth"]["hasBulletins"].GetBool());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build stats for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  processor->getMetrics()->invocations() = 1;
  processor->getMetrics()->incomingFlowFiles() = 2;
  processor->getMetrics()->bytesRead() = 3;
  processor->getMetrics()->bytesWritten() = 4;
  processor->getMetrics()->transferredFlowFiles() = 5;
  processor->getMetrics()->processingNanos() = 6;
  processor->getMetrics()->incomingBytes() = 7;
  processor->getMetrics()->transferredBytes() = 8;
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:stats"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["bulletinList"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["flowfilesReceived"].GetInt64() == 2);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesRead"].GetInt64() == 3);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesWritten"].GetInt64() == 4);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["flowfilesSent"].GetInt64() == 5);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["invocations"].GetInt64() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["processingNanos"].GetInt64() == 6);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesReceived"].GetInt64() == 7);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesTransferred"].GetInt64() == 8);
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build bulletins for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  auto processor_ptr = processor.get();
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto conf = std::make_shared<minifi::ConfigureImpl>();
  auto now = std::chrono::system_clock::now();
  auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  core::BulletinStore bulletin_store(*conf);
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::err, "error message");
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::critical, "critical message");
  flow_status_builder.setBulletinStore(&bulletin_store);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:health,bulletins"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"]["hasBulletins"].GetBool());
  auto bulletin_array = status["processorStatusList"].GetArray()[0]["bulletinList"].GetArray();
  CHECK(bulletin_array.Size() == 2);
  CHECK(bulletin_array[0]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[0]["message"].GetString() == std::string{"error message"});
  CHECK(bulletin_array[1]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[1]["message"].GetString() == std::string{"critical message"});
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for all processors", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor1 = test::utils::make_processor<DummyProcessor>("DummyProcessor1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5"));
  auto processor2 = test::utils::make_processor<DummyProcessor>("DummyProcessor2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5"));
  process_group.addProcessor(std::move(processor1));
  process_group.addProcessor(std::move(processor2));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:all:health"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 2);
  std::unordered_set<std::string> expected_processor_ids = {"123fa7e6-2459-46dd-b2ba-61517239edf5", "456fa7e6-2459-46dd-b2ba-61517239edf5"};
  std::unordered_set<std::string> expected_processor_names = {"DummyProcessor1", "DummyProcessor2"};
  for (const auto& processor_status : status["processorStatusList"].GetArray()) {
    auto id = processor_status["id"].GetString();
    auto name = processor_status["name"].GetString();
    CHECK(expected_processor_ids.contains(id));
    CHECK(expected_processor_names.contains(name));
    expected_processor_ids.erase(id);
    expected_processor_names.erase(name);
  }
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Non-existent processor generates an error", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:InvalidProcessor:health"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].Empty());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get processorStatus: No processor with key 'InvalidProcessor' to report status on"});
}

TEST_CASE("Build processor status with only non-existent options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  REQUIRE_THROWS_WITH(flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:invalid1,invalid2"}}), "Invalid query option: invalid1");
}

TEST_CASE("Build processor status with invalid option", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:processorstats"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].Empty());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get processorStatus: Invalid query option for processor status 'processorstats'"});
}

TEST_CASE("Building processor status fails with incomplete query", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor::"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].Empty());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get processorStatus: Query is incomplete"});
}

TEST_CASE("Build health status for single connection", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1:health"}});
  REQUIRE(status["connectionStatusList"].GetArray().Size() == 1);
  CHECK(status["connectionStatusList"].GetArray()[0]["id"] == "123fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["connectionStatusList"].GetArray()[0]["name"] == "Conn1");
  CHECK(status["connectionStatusList"].GetArray()[0]["connectionHealth"]["queuedCount"].GetInt64() == 1);
  CHECK(status["connectionStatusList"].GetArray()[0]["connectionHealth"]["queuedBytes"].GetInt64() == 0);
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for all connections", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection1 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  auto connection2 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files1{std::make_shared<core::FlowFileImpl>()};
  connection1->multiPut(flow_files1);
  std::vector<std::shared_ptr<core::FlowFile>> flow_files2{std::make_shared<core::FlowFileImpl>(), std::make_shared<core::FlowFileImpl>()};
  connection2->multiPut(flow_files2);
  process_group.addConnection(std::move(connection1));
  process_group.addConnection(std::move(connection2));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:all:health"}});
  REQUIRE(status["connectionStatusList"].GetArray().Size() == 2);
  for (const auto& connection_status : status["connectionStatusList"].GetArray()) {
    std::string id = connection_status["id"].GetString();
    std::string name = connection_status["name"].GetString();
    bool id_and_name_check = (id == "123fa7e6-2459-46dd-b2ba-61517239edf5" && name == "Conn1") || (id == "456fa7e6-2459-46dd-b2ba-61517239edf5" && name == "Conn2");
    CHECK(id_and_name_check);
    if (id == "123fa7e6-2459-46dd-b2ba-61517239edf5") {
      CHECK(connection_status["connectionHealth"]["queuedCount"].GetInt64() == 1);
      CHECK(connection_status["connectionHealth"]["queuedBytes"].GetInt64() == 0);
    } else {
      CHECK(connection_status["connectionHealth"]["queuedCount"].GetInt64() == 2);
      CHECK(connection_status["connectionHealth"]["queuedBytes"].GetInt64() == 0);
    }
  }
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Non-existent connection generates an error", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:InvalidConnection:health"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].Empty());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get connectionStatus: No connection with key 'InvalidConnection' to report status on"});
}

TEST_CASE("Build connection status with only non-existent options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  REQUIRE_THROWS_WITH(flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1:invalid1,invalid2"}}), "Invalid query option: invalid1");
}

TEST_CASE("Build connection status with invalid option", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1:stats"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].Empty());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get connectionStatus: Invalid query option for connection status 'stats'"});
}

TEST_CASE("Building connection status fails with incomplete query", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].Empty());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get connectionStatus: Query is incomplete"});
}

TEST_CASE("Test non-existent instance status options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  REQUIRE_THROWS_WITH(flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"instance:invalid1,invalid2"}}), "Invalid query option: invalid1");
}

TEST_CASE("Test invalid instance status options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"instance:contentrepositoryusage"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get instance: Invalid query option for instance status 'contentrepositoryusage'"});
}

TEST_CASE("Build instance health and bulletin list", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5"));
  auto processor_ptr = processor.get();
  process_group.addProcessor(std::move(processor));
  auto conf = std::make_shared<minifi::ConfigureImpl>();
  auto now = std::chrono::system_clock::now();
  auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  core::BulletinStore bulletin_store(*conf);
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::err, "error message");
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::critical, "critical message");
  flow_status_builder.setBulletinStore(&bulletin_store);

  auto connection1 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  auto connection2 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files1{std::make_shared<core::FlowFileImpl>()};
  connection1->multiPut(flow_files1);
  std::vector<std::shared_ptr<core::FlowFile>> flow_files2{std::make_shared<core::FlowFileImpl>(), std::make_shared<core::FlowFileImpl>()};
  connection2->multiPut(flow_files2);
  process_group.addConnection(std::move(connection1));
  process_group.addConnection(std::move(connection2));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"instance:health,bulletins"}});
  REQUIRE(status["instanceStatus"].GetObject().HasMember("instanceHealth"));
  CHECK(status["instanceStatus"]["instanceHealth"]["queuedCount"].GetInt64() == 3);
  CHECK(status["instanceStatus"]["instanceHealth"]["queuedContentSize"].GetInt64() == 0);
  CHECK(status["instanceStatus"]["instanceHealth"]["hasBulletins"].GetBool());
  REQUIRE(status["instanceStatus"].GetObject().HasMember("bulletinList"));
  auto bulletin_array = status["instanceStatus"]["bulletinList"].GetArray();
  CHECK(bulletin_array.Size() == 2);
  CHECK(bulletin_array[0]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[0]["message"].GetString() == std::string{"error message"});
  CHECK(bulletin_array[1]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[1]["message"].GetString() == std::string{"critical message"});
  CHECK(status["instanceStatus"]["instanceStats"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build instance stats", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor1 = test::utils::make_processor<DummyProcessor>("DummyProcessor1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5"));
  auto processor2 = test::utils::make_processor<DummyProcessor>("DummyProcessor2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5"));
  processor1->getMetrics()->bytesRead() = 1;
  processor1->getMetrics()->bytesWritten() = 2;
  processor1->getMetrics()->transferredFlowFiles() = 3;
  processor1->getMetrics()->transferredBytes() = 4;
  process_group.addProcessor(std::move(processor1));
  processor2->getMetrics()->bytesRead() = 5;
  processor2->getMetrics()->bytesWritten() = 6;
  processor2->getMetrics()->transferredFlowFiles() = 7;
  processor2->getMetrics()->transferredBytes() = 8;
  process_group.addProcessor(std::move(processor2));
  flow_status_builder.setRoot(&process_group);

  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"instance:stats"}});
  REQUIRE(status["instanceStatus"].GetObject().HasMember("instanceStats"));
  CHECK(status["instanceStatus"]["instanceStats"]["bytesRead"].GetInt64() == 6);
  CHECK(status["instanceStatus"]["instanceStats"]["bytesWritten"].GetInt64() == 8);
  CHECK(status["instanceStatus"]["instanceStats"]["bytesTransferred"].GetInt64() == 12);
  CHECK(status["instanceStatus"]["instanceStats"]["flowfilesTransferred"].GetInt64() == 10);
  CHECK(status["instanceStatus"]["instanceHealth"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Test non-existent system diagnostics status options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  REQUIRE_THROWS_WITH(flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"systemdiagnostics:invalid1,invalid2"}}), "Invalid query option: invalid1");
}

TEST_CASE("Test invalid system diagnostics options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"systemdiagnostics:health"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get systemDiagnostics: Invalid query option for system diagnostics 'health'"});
}

TEST_CASE("Build system diagnostics processorStatus", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"systemdiagnostics:processorstats"}});
  REQUIRE(status["systemDiagnosticsStatus"].GetObject().HasMember("processorStatus"));
#ifndef WIN32
  CHECK(status["systemDiagnosticsStatus"]["processorStatus"]["loadAverage"].GetDouble() > 0);
#endif
  CHECK(status["systemDiagnosticsStatus"]["processorStatus"]["availableProcessors"].GetInt64() >= 1);
}

TEST_CASE("Build system diagnostics disk utilization", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  TestController test_controller;
  auto output_dir = test_controller.createTempDirectory();
  flow_status_builder.setRepositoryPaths(output_dir, output_dir);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"systemdiagnostics:contentrepositoryusage,flowfilerepositoryusage"}});
  REQUIRE(status["systemDiagnosticsStatus"].GetObject().HasMember("flowfileRepositoryUsage"));
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["freeSpace"].GetInt64() > 0);
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["totalSpace"].GetInt64() > 0);
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["usedSpace"].GetInt64() > 0);
  auto disk_utilization = status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["diskUtilization"].GetInt64();
  CHECK((disk_utilization < 100 && disk_utilization > 0));
  REQUIRE(status["systemDiagnosticsStatus"].GetObject().HasMember("contentRepositoryUsage"));
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["freeSpace"].GetInt64() > 0);
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["totalSpace"].GetInt64() > 0);
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["usedSpace"].GetInt64() > 0);
  disk_utilization = status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["diskUtilization"].GetInt64();
  CHECK((disk_utilization < 100 && disk_utilization > 0));
}

TEST_CASE("Build system diagnostics disk utilization if no repository path is available", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"systemdiagnostics:contentrepositoryusage,flowfilerepositoryusage"}});
  REQUIRE(status["systemDiagnosticsStatus"].GetObject().HasMember("flowfileRepositoryUsage"));
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["freeSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["totalSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["usedSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["flowfileRepositoryUsage"]["diskUtilization"].GetInt64() == -1);
  REQUIRE(status["systemDiagnosticsStatus"].GetObject().HasMember("contentRepositoryUsage"));
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["freeSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["totalSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["usedSpace"].GetInt64() == -1);
  CHECK(status["systemDiagnosticsStatus"]["contentRepositoryUsage"]["diskUtilization"].GetInt64() == -1);
}

}  // namespace org::apache::nifi::minifi::test
