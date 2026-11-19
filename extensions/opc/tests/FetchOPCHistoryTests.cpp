// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <optional>
#include <string>
#include <vector>

#include "OpcUaTestServer.h"
#include "catch2/generators/catch_generators.hpp"
#include "include/FetchOPCHistory.h"
#include "rapidjson/document.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

class FetchOPCHistoryTestController {
 public:
  FetchOPCHistoryTestController()
      : controller_(minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")),
        processor_(controller_.getProcessor()) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::FetchOPCHistory>();
  }

  void setupProcessor(const std::string& node_id_type, const std::string& node_id) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4842/"));
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::NodeIDType.name, node_id_type));
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::NodeID.name, node_id));
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

  void checkFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::string& content, const std::string& node_id,
      const std::string& source_timestamp) {
    CHECK(controller_.plan->getContent(flow_file) == content);
    CHECK(flow_file->getAttribute(processors::FetchOPCHistory::NodeIDAttr.name) == node_id);
    CHECK(flow_file->getAttribute(processors::FetchOPCHistory::NamespaceIndexAttr.name) == std::to_string(server_.getNamespaceIndex()));
    CHECK(flow_file->getAttribute(processors::FetchOPCHistory::SourcetimestampAttr.name) == source_timestamp);
  }

  static void checkModificationAttributes(const std::shared_ptr<core::FlowFile>& flow_file, bool present, const std::string& username = "",
      const std::string& update_type = "", const std::string& modification_time = "") {
    if (present) {
      CHECK(flow_file->getAttribute("modification.username") == username);
      CHECK(flow_file->getAttribute("modification.update.type") == update_type);
      CHECK(flow_file->getAttribute("modification.time") == modification_time);
    } else {
      CHECK(flow_file->getAttribute("modification.username") == std::nullopt);
      CHECK(flow_file->getAttribute("modification.update.type") == std::nullopt);
      CHECK(flow_file->getAttribute("modification.time") == std::nullopt);
    }
  }

  void verifyResults(const ProcessorTriggerResult& results, const std::string& expected_contents) const {
    auto& fetch_results = results.at(processors::FetchOPCHistory::Success);
    REQUIRE(fetch_results.size() == 1);
    rapidjson::Document result_document;
    result_document.Parse(controller_.plan->getContent(fetch_results[0]).c_str());
    rapidjson::Document expected_document;
    expected_document.Parse(expected_contents.c_str());
    REQUIRE(result_document == expected_document);
  }

 protected:
  minifi::test::SingleProcessorTestController controller_;
  core::Processor* processor_;
  OpcUaTestServer server_{4842};
};

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of node with a single entry", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT1");
  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "1", "INT1", "2024-06-15T10:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "test_user", "Replace", "2024-06-15T10:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of node with a single integer nodeid entry", "[fetchopchistory]") {
  server_.start();
  setupProcessor("Int", "666");
  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "256", "666", "2001-01-01T22:22:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "integer_user", "Insert", "2001-01-01T22:22:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test invalid timestamps", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  SECTION("Invalid StartTimestamp") {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::StartTimestamp.name, "2025-10-01T00:00:00ZZ"));
    REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Invalid StartTimestamp: 2025-10-01T00:00:00ZZ");
  }

  SECTION("Invalid EndTimestamp") {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::EndTimestamp.name, "22025-10-01T00:00:00Z"));
    REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Invalid EndTimestamp: 22025-10-01T00:00:00Z");
  }
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history after a specific timestamp", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::StartTimestamp.name, "2025-10-01T00:00:00Z"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "3", "INT2", "2025-11-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Update", "2025-11-11T11:30:00.000Z");

  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  checkFlowFile(flow_file, "4", "INT2", "2026-03-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "test_user", "Replace", "2026-03-11T11:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history before a specific timestamp", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::EndTimestamp.name, "2025-11-12T00:00:00Z"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "2", "INT2", "2021-03-15T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Insert", "2021-03-15T11:30:00.000Z");

  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  checkFlowFile(flow_file, "3", "INT2", "2025-11-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Update", "2025-11-11T11:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test batch size limit", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::BatchSize.name, "2"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "2", "INT2", "2021-03-15T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Insert", "2021-03-15T11:30:00.000Z");

  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  checkFlowFile(flow_file, "3", "INT2", "2025-11-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Update", "2025-11-11T11:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test batch size of zero returns all available entries", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::BatchSize.name, "0"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 3);
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[0], "2", "INT2", "2021-03-15T11:30:00.000Z");
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[1], "3", "INT2", "2025-11-11T11:30:00.000Z");
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[2], "4", "INT2", "2026-03-11T11:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test triggering again after all entries have been fetched returns nothing", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");

  auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 3);

  results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCHistory::Success).empty());
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of a non-existing node yields no flow files", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "NONEXISTENT");

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCHistory::Success).empty());
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test using a non-numeric node ID with the Int node ID type throws on schedule",
    "[fetchopchistory]") {
  server_.start();
  setupProcessor("Int", "not_a_number");

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: not_a_number cannot be used as an int type node ID");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test entries sharing a timestamp are all delivered once and then deduplicated",
    "[fetchopchistory]") {
  const auto shared_time = OpcUaTestServer::makeDateTime(2023, 1, 1, 0, 0, 0, 0);
  server_.setHistory("INT3",
      {HistoryModificationRecord{.value = 10, .username = "user", .update_type = UA_HISTORYUPDATETYPE_INSERT, .modification_time = shared_time},
          HistoryModificationRecord{.value = 20, .username = "user", .update_type = UA_HISTORYUPDATETYPE_INSERT, .modification_time = shared_time}});
  server_.start();
  setupProcessor("String", "INT3");

  auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[0], "10", "INT3", "2023-01-01T00:00:00.000Z");
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[1], "20", "INT3", "2023-01-01T00:00:00.000Z");

  results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCHistory::Success).empty());
}

TEST_CASE_METHOD(FetchOPCHistoryTestController,
    "Test entries sharing the boundary timestamp are not re-fetched when the history is paginated across callbacks", "[fetchopchistory]") {
  const auto shared_time = OpcUaTestServer::makeDateTime(2023, 1, 1, 0, 0, 0, 0);
  server_.setHistory("INT3",
      {HistoryModificationRecord{.value = 10, .username = "user", .update_type = UA_HISTORYUPDATETYPE_INSERT, .modification_time = shared_time},
          HistoryModificationRecord{.value = 20, .username = "user", .update_type = UA_HISTORYUPDATETYPE_INSERT, .modification_time = shared_time}});
  server_.setHistoryPageSize(1);
  server_.start();
  setupProcessor("String", "INT3");

  auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[0], "10", "INT3", "2023-01-01T00:00:00.000Z");
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[1], "20", "INT3", "2023-01-01T00:00:00.000Z");

  results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCHistory::Success).empty());
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test non-existing record set writer", "[fetchopchistory]") {
  server_.start();
  auto json_record_set_writer = controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller_.plan->setProperty(json_record_set_writer, "Output Grouping", "One Line Per Object"));
  setupProcessor("String", "INT1");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "InvalidRecordSetWriter"));

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Controller service 'InvalidRecordSetWriter' not found");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test RecordSetWriter with JSON output format", "[fetchopchistory]") {
  server_.start();
  auto json_record_set_writer = controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller_.plan->setProperty(json_record_set_writer, "Output Grouping", "One Line Per Object"));
  setupProcessor("String", "INT1");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "JsonRecordSetWriter"));

  std::string expected_json_content;
  SECTION("Fetch full history") {
    expected_json_content = R"({"value":"1","source.timestamp":"2024-06-15T10:30:00.000Z","node.id":"INT1","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\","
        R"("modification.username":"test_user","modification.update.type":"Replace","modification.time":"2024-06-15T10:30:00.000Z"})";
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  SECTION("Fetch raw history") {
    expected_json_content = R"({"value":"1","source.timestamp":"2024-06-15T10:30:00.000Z","node.id":"INT1","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) + "\"}";
  }

  const auto results = controller_.trigger();
  verifyResults(results, expected_json_content);
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test RecordSetWriter with JSON output format with multiple values", "[fetchopchistory]") {
  server_.start();
  auto json_record_set_writer = controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "JsonRecordSetWriter"));

  std::string expected_json_content;
  SECTION("Fetch full history") {
    expected_json_content = R"([{"value":"2","source.timestamp":"2021-03-15T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\","
        R"("modification.username":"admin_user","modification.update.type":"Insert","modification.time":"2021-03-15T11:30:00.000Z"}, )"
        R"({"value":"3","source.timestamp":"2025-11-11T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\","
        R"("modification.username":"admin_user","modification.update.type":"Update","modification.time":"2025-11-11T11:30:00.000Z"}, )"
        R"({"value":"4","source.timestamp":"2026-03-11T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\","
        R"("modification.username":"test_user","modification.update.type":"Replace","modification.time":"2026-03-11T11:30:00.000Z"}])";
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  SECTION("Fetch raw history") {
    expected_json_content = R"([{"value":"2","source.timestamp":"2021-03-15T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\"},"
        R"({"value":"3","source.timestamp":"2025-11-11T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) +
        "\"},"
        R"({"value":"4","source.timestamp":"2026-03-11T11:30:00.000Z","node.id":"INT2","namespace.index":")" +
        std::to_string(server_.getNamespaceIndex()) + "\"}]";
  }

  const auto results = controller_.trigger();
  verifyResults(results, expected_json_content);
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test byte string value is written to the flow file content as raw bytes", "[fetchopchistory]") {
  const std::vector<uint8_t> bytes{0x00, 0x0f, 0xa0, 0xff};
  server_.setHistory("INT3",
      {HistoryModificationRecord{.username = "byte_user",
          .update_type = UA_HISTORYUPDATETYPE_INSERT,
          .modification_time = OpcUaTestServer::makeDateTime(2023, 1, 1, 0, 0, 0, 0),
          .byte_string_value = bytes}});
  server_.start();
  setupProcessor("String", "INT3");

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  checkFlowFile(results.at(processors::FetchOPCHistory::Success)[0],
      std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()},
      "INT3",
      "2023-01-01T00:00:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test byte string value is base64 encoded in a record", "[fetchopchistory]") {
  server_.setHistory("INT3",
      {HistoryModificationRecord{.username = "byte_user",
          .update_type = UA_HISTORYUPDATETYPE_INSERT,
          .modification_time = OpcUaTestServer::makeDateTime(2023, 1, 1, 0, 0, 0, 0),
          .byte_string_value = std::vector<uint8_t>{0x00, 0x0f, 0xa0, 0xff}}});
  server_.start();
  auto json_record_set_writer = controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller_.plan->setProperty(json_record_set_writer, "Output Grouping", "One Line Per Object"));
  setupProcessor("String", "INT3");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "JsonRecordSetWriter"));

  const auto results = controller_.trigger();
  verifyResults(results,
      R"({"value":"AA+g/w==","source.timestamp":"2023-01-01T00:00:00.000Z","node.id":"INT3","namespace.index":")" +
          std::to_string(server_.getNamespaceIndex()) + "\"}");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test multiple triggers with state kept in state manager", "[fetchopchistory]") {
  server_.start();
  setupProcessor("String", "INT2");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::BatchSize.name, "1"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "2", "INT2", "2021-03-15T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Insert", "2021-03-15T11:30:00.000Z");

  results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "3", "INT2", "2025-11-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "admin_user", "Update", "2025-11-11T11:30:00.000Z");

  results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "4", "INT2", "2026-03-11T11:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "test_user", "Replace", "2026-03-11T11:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of a node with a GUID node ID type", "[fetchopchistory]") {
  server_.start();
  setupProcessor("Guid", "72962b91-fa75-4ae6-8d28-b404dc7daf63");
  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "7", "72962b91-fa75-4ae6-8d28-b404dc7daf63", "2020-05-20T12:00:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "guid_user", "Insert", "2020-05-20T12:00:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test using an invalid GUID with the Guid node ID type throws on schedule", "[fetchopchistory]") {
  server_.start();
  setupProcessor("Guid", "not-a-guid");

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: not-a-guid cannot be used as a GUID type node ID");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of a node resolved from a path node ID type", "[fetchopchistory]") {
  server_.start();
  setupProcessor("Path", "Simulator/Default/Device1/INT1");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::PathReferenceTypes.name, "Organizes/Organizes/HasComponent"));
  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(processor_->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Audit"));
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  checkFlowFile(flow_file, "1", "Simulator/Default/Device1/INT1", "2024-06-15T10:30:00.000Z");
  checkModificationAttributes(flow_file, contains_modification_attributes, "test_user", "Replace", "2024-06-15T10:30:00.000Z");
}

TEST_CASE_METHOD(FetchOPCHistoryTestController, "Test fetching history of a non-existing path yields no flow files", "[fetchopchistory]") {
  server_.start();
  setupProcessor("Path", "Simulator/Default/Nope");
  REQUIRE(processor_->setProperty(processors::FetchOPCHistory::PathReferenceTypes.name, "Organizes/Organizes"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCHistory::Success).empty());
}

}  // namespace org::apache::nifi::minifi::test
