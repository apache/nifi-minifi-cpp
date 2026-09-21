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
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "OpcUaTestServer.h"
#include "catch2/generators/catch_generators.hpp"
#include "include/FetchOPCEvents.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

class FetchOPCEventsTestController {
 public:
  FetchOPCEventsTestController()
      : controller_(minifi::test::utils::make_processor<processors::FetchOPCEvents>("FetchOPCEvents")),
        processor_(controller_.getProcessor()) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::FetchOPCEvents>();
  }

  void setupProcessor(const std::string& node_id_type, const std::string& node_id) {
    REQUIRE(controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter"));
    REQUIRE(controller_.plan->setProperty(processor_, processors::FetchOPCEvents::RecordSetWriter.name, "JsonRecordSetWriter"));
    REQUIRE(processor_->setProperty(processors::FetchOPCEvents::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4843/"));
    REQUIRE(processor_->setProperty(processors::FetchOPCEvents::NodeIDType.name, node_id_type));
    REQUIRE(processor_->setProperty(processors::FetchOPCEvents::NodeID.name, node_id));
    REQUIRE(processor_->setProperty(processors::FetchOPCEvents::NameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

  void waitForSubscription() {
    CHECK(controller_.trigger().at(processors::FetchOPCEvents::Success).empty());
    REQUIRE(utils::verifyLogLinePresenceInPollTime(10s, "Subscribed to the events of node"));
  }

  // The processor reports the events dropped since the previous trigger, so the total has to be summed up over all the warnings.
  static size_t countDroppedEvents() {
    const std::regex dropped_event_pattern{R"(The event queue is full, (\d+) OPC UA events were dropped)"};
    const auto logs = LogTestController::getInstance().getLogs();
    size_t dropped_count = 0;
    for (auto match = std::sregex_iterator{logs.begin(), logs.end(), dropped_event_pattern}; match != std::sregex_iterator{}; ++match) {
      dropped_count += std::stoul((*match)[1].str());
    }
    return dropped_count;
  }

  void verifyResults(const ProcessorTriggerResult& results, const std::vector<std::string>& expected_contents) const {
    const auto& fetch_results = results.at(processors::FetchOPCEvents::Success);
    REQUIRE(fetch_results.size() == expected_contents.size());

    for (size_t i = 0; i < expected_contents.size(); ++i) {
      rapidjson::Document result_document;
      result_document.Parse(controller_.plan->getContent(fetch_results[i]).c_str());
      rapidjson::Document expected_document;
      expected_document.Parse(expected_contents[i].c_str());

      auto array = expected_document.GetArray();
      for (rapidjson::SizeType j = 0; j < array.Size(); ++j) {
        for (const auto& field : array[j].GetObject()) {
          if (field.value == ".*") {
            REQUIRE(result_document[j][field.name].IsString());
            result_document[j][field.name].SetString(".*", result_document.GetAllocator());
          }
        }
      }

      REQUIRE(result_document == expected_document);
    }
  }

 protected:
  minifi::test::SingleProcessorTestController controller_;
  core::Processor* processor_;
  OpcUaTestServer server_{4843};
};

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching events of a non-existent node", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "non-existent");

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find("Failed to subscribe to the events of node 'non-existent': BadNodeIdUnknown") != std::string::npos;
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching events of an existing node but in the wrong namespace", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::NameSpaceIndex.name, "7979"));

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find("Failed to subscribe to the events of node 'EventSource': BadNodeIdUnknown") != std::string::npos;
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test non-existent record set writer", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(controller_.plan->setProperty(processor_, processors::FetchOPCEvents::RecordSetWriter.name, "InvalidRecordSetWriter"));

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Controller service 'InvalidRecordSetWriter' not found");
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching a single event of a node", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  waitForSubscription();

  server_.triggerEvent("test event", 500);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  const auto& flow_files = result.at(processors::FetchOPCEvents::Success);
  REQUIRE(flow_files.size() == 1);
  verifyResults(result, {R"([{"EventId":".*","Message":"test event","Severity":"500","SourceName":"EventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Events arriving between triggers are all forwarded", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  waitForSubscription();

  constexpr size_t event_count = 5;
  for (size_t i = 0; i < event_count; ++i) {
    server_.triggerEvent(fmt::format("event {}", i), 500);
  }

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  const auto& flow_files = result.at(processors::FetchOPCEvents::Success);
  REQUIRE(flow_files.size() == 1);
  auto& flow_file = flow_files[0];
  std::set<std::string> messages;

  rapidjson::Document document;
  document.Parse(controller_.plan->getContent(flow_file).c_str());
  for (auto& event : document.GetArray()) {
    auto event_obj = event.GetObject();
    messages.insert(event_obj["Message"].GetString());
  }

  CHECK(messages == std::set<std::string>{"event 0", "event 1", "event 2", "event 3", "event 4"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Events over the queue size limit are dropped and reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::MaxQueueSize.name, "2"));
  waitForSubscription();

  constexpr size_t event_count = 5;
  for (size_t i = 0; i < event_count; ++i) {
    server_.triggerEvent(fmt::format("event {}", i), 500);
  }

  // Each trigger both reports the events dropped since the last one and drains the queue, so the two counts together must
  // account for every event the server raised.
  std::vector<std::string> delivered_messages;
  size_t dropped_count = 0;
  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    const auto triggered = controller_.trigger();
    for (const auto& flow_file : triggered.at(processors::FetchOPCEvents::Success)) {
      rapidjson::Document document;
      document.Parse(controller_.plan->getContent(flow_file).c_str());
      for (const auto& event : document.GetArray()) {
        delivered_messages.emplace_back(event.GetObject()["Message"].GetString());
      }
    }
    dropped_count = countDroppedEvents();
    return delivered_messages.size() + dropped_count == event_count;
  }, 100ms));

  CHECK(dropped_count > 0);
  // The oldest events are the ones dropped, so the events left are the newest ones and they keep the order they were raised in.
  CHECK(std::ranges::is_sorted(delivered_messages));
  CHECK(delivered_messages.back() == fmt::format("event {}", event_count - 1));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching multiple events with a batch size limit", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::BatchSize.name, "2"));
  waitForSubscription();

  server_.triggerEvent("test event 1", 500);
  server_.triggerEvent("test event 2", 300);
  server_.triggerEvent("test event 3", 200);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 2}}, result, 10s));

  const auto& flow_files = result.at(processors::FetchOPCEvents::Success);
  REQUIRE(flow_files.size() == 2);
  verifyResults(result, {R"([{"EventId":".*","Message":"test event 1","Severity":"500","SourceName":"EventSource","EventType":"i=2041","Time":".*"},)"
                         R"({"EventId":".*","Message":"test event 2","Severity":"300","SourceName":"EventSource","EventType":"i=2041","Time":".*"}])",
                         R"([{"EventId":".*","Message":"test event 3","Severity":"200","SourceName":"EventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching events with minimum severity limit", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::MinimumSeverity.name, "300"));
  waitForSubscription();

  server_.triggerEvent("test event 1", 500);
  server_.triggerEvent("test event 2", 300);
  server_.triggerEvent("test event 3", 200);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  const auto& flow_files = result.at(processors::FetchOPCEvents::Success);
  REQUIRE(flow_files.size() == 1);
  verifyResults(result, {R"([{"EventId":".*","Message":"test event 1","Severity":"500","SourceName":"EventSource","EventType":"i=2041","Time":".*"},)"
                         R"({"EventId":".*","Message":"test event 2","Severity":"300","SourceName":"EventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Only the selected fields are requested and they name the record fields", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message, /Severity"));
  waitForSubscription();

  server_.triggerEvent("test event", 500);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"test event","Severity":"500"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Only the events of the selected event type are reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  // i=2052 is AuditEventType, a subtype of BaseEventType
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::EventTypeNodeId.name, "i=2052"));
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message,EventType"));
  waitForSubscription();

  server_.triggerEvent("base event", 500);
  server_.triggerEvent("audit event", 500, UA_NS0ID_AUDITEVENTTYPE);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"audit event","EventType":"i=2052"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "The event filter expression overrides the other filter properties", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "EventId,SourceName,Time"));
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::EventTypeNodeId.name, "i=2052"));
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::MinimumSeverity.name, "900"));
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::EventFilterExpression.name, "SELECT /Message, /Severity WHERE /Severity >= 400"));
  waitForSubscription();

  server_.triggerEvent("too quiet", 300);
  server_.triggerEvent("loud enough", 500);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"loud enough","Severity":"500"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "An unparsable event filter expression is reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::EventFilterExpression.name, "PICK /Message WHENEVER /Severity"));

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find("Failed to parse the event filter 'PICK /Message WHENEVER /Severity'") != std::string::npos;
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Selecting no fields at all is rejected", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, " , "));

  REQUIRE_THROWS_WITH(controller_.trigger(),
      "Process Schedule Operation: At least one field must be set in 'Select fields', otherwise the events would carry no data");
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "A select field can be a nested path or a namespace qualified browse name", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message,Tool/Diameter,1:Colour"));
  waitForSubscription();

  server_.triggerEventWithFields("nested event", 500, {{"/Tool/Diameter", "42mm"}, {"/1:Colour", "red"}});

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"nested event","Tool/Diameter":"42mm","1:Colour":"red"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "A select field can be prefixed with the node ID of an event type", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");

  // i=2052 is AuditEventType and ActionTimeStamp is one of its fields, so the server accepts this clause.
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message,i=2052/ActionTimeStamp"));
  waitForSubscription();

  server_.triggerEventWithFields("prefixed event", 500, {{"i=2052/ActionTimeStamp", "2026-09-24T10:00:00Z"}});

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"prefixed event","i=2052/ActionTimeStamp":"2026-09-24T10:00:00Z"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "The event subscription is replaced after the connection to the server is lost", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  waitForSubscription();

  server_.stop();
  server_.start();
  REQUIRE(utils::verifyLogLinePresenceInPollTime(30s, "Deleting the dead OPC UA event subscription 1 before resubscribing"));

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(utils::verifyEventHappenedInPollTime(30s, [&]() {
    server_.triggerEvent("event after the restart", 500);
    result = controller_.trigger();
    return !result.at(processors::FetchOPCEvents::Success).empty();
  }, 200ms));
  CHECK(controller_.plan->getContent(result.at(processors::FetchOPCEvents::Success)[0]).find("event after the restart") != std::string::npos);
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching a single event of a node addressed by its path", "[fetchopcevents]") {
  server_.start();
  setupProcessor("Path", "EventSource");
  waitForSubscription();

  server_.triggerEvent("path addressed event", 500);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"EventId":".*","Message":"path addressed event","Severity":"500","SourceName":"EventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "A path that cannot be resolved to a node is reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("Path", "Simulator/Default/NoSuchNode");

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find(
        "Failed to translate path 'Simulator/Default/NoSuchNode' to a node id: BadNoDataAvailable") != std::string::npos;
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "A path that resolves to multiple nodes is reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("Path", "Simulator/Default/AmbiguousParent/Ambiguous");

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find(
        "Path 'Simulator/Default/AmbiguousParent/Ambiguous' resolved to 2 node ids; exactly one is required to subscribe to events") != std::string::npos;
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching a single event of a node addressed by an int node ID", "[fetchopcevents]") {
  server_.start();
  setupProcessor("Int", "4200");
  waitForSubscription();

  server_.triggerEvent("int addressed event", 500, UA_NS0ID_BASEEVENTTYPE, opc::OPCNodeIDType::Int);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result,
      {R"([{"EventId":".*","Message":"int addressed event","Severity":"500","SourceName":"IntEventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "Test fetching a single event of a node addressed by a GUID node ID", "[fetchopcevents]") {
  server_.start();
  setupProcessor("Guid", "aabbccdd-eeff-0a1b-2c3d-4e5f6a7b8c9d");
  waitForSubscription();

  server_.triggerEvent("GUID addressed event", 500, UA_NS0ID_BASEEVENTTYPE, opc::OPCNodeIDType::Guid);

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result,
      {R"([{"EventId":".*","Message":"GUID addressed event","Severity":"500","SourceName":"GuidEventSource","EventType":"i=2041","Time":".*"}])"});
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "An event field that cannot be converted to a string is skipped and reported", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message,Tool/Range"));
  waitForSubscription();

  server_.triggerEventWithUnconvertibleField("event with a range field", 500, "/Tool/Range");

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(controller_.triggerUntil({{processors::FetchOPCEvents::Success, 1}}, result, 10s));

  verifyResults(result, {R"([{"Message":"event with a range field"}])"});
  CHECK(LogTestController::getInstance().getLogs().find("Failed to convert event field 'Tool/Range' to string, skipping field") != std::string::npos);
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "The event subscription is recreated after the processor is stopped and started again", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  waitForSubscription();

  // Unschedules the processor, which stops the event thread and drops the client, then makes the next trigger schedule it again.
  controller_.plan->reset(true);
  LogTestController::getInstance().clear();
  waitForSubscription();

  minifi::test::ProcessorTriggerResult result;
  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    server_.triggerEvent("event after the restart", 500);
    result = controller_.trigger();
    return !result.at(processors::FetchOPCEvents::Success).empty();
  }, 200ms));
  CHECK(controller_.plan->getContent(result.at(processors::FetchOPCEvents::Success)[0]).find("event after the restart") != std::string::npos);
}

TEST_CASE_METHOD(FetchOPCEventsTestController, "A select field of an event type unknown to the server is rejected", "[fetchopcevents]") {
  server_.start();
  setupProcessor("String", "EventSource");
  REQUIRE(processor_->setProperty(processors::FetchOPCEvents::SelectFields.name, "Message,ns=1;s=NoSuchEventType/Diameter"));

  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    controller_.trigger();
    return LogTestController::getInstance().getLogs().find("Failed to subscribe to the events of node 'EventSource': BadEventFilterInvalid") !=
           std::string::npos;
  }, 100ms));
}

}  // namespace org::apache::nifi::minifi::test
