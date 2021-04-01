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

#include "ConsumeWindowsEventLog.h"

#include "core/ConfigurableComponent.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "TestBase.h"
#include "utils/TestUtils.h"
#include "utils/file/FileUtils.h"
#include "rapidjson/document.h"

#include "CWELTestUtils.h"
#include "Utils.h"

using ConsumeWindowsEventLog = org::apache::nifi::minifi::processors::ConsumeWindowsEventLog;
using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;
using PutFile = org::apache::nifi::minifi::processors::PutFile;
using ConfigurableComponent = org::apache::nifi::minifi::core::ConfigurableComponent;
using IdGenerator = org::apache::nifi::minifi::utils::IdGenerator;

namespace {

const std::string APPLICATION_CHANNEL = "Application";

constexpr DWORD CWEL_TESTS_OPCODE = 14985;  // random opcode hopefully won't clash with something important
const std::string QUERY = "Event/System/EventID=" + std::to_string(CWEL_TESTS_OPCODE);

void reportEvent(const std::string& channel, const char* message, WORD log_level = EVENTLOG_INFORMATION_TYPE) {
  auto event_source = RegisterEventSourceA(nullptr, channel.c_str());
  auto deleter = gsl::finally([&event_source](){ DeregisterEventSource(event_source); });
  ReportEventA(event_source, log_level, 0, CWEL_TESTS_OPCODE, nullptr, 1, 0, &message, nullptr);
}

class SimpleFormatTestController : public OutputFormatTestController {
 public:
  using OutputFormatTestController::OutputFormatTestController;

 protected:
  void dispatchBookmarkEvent() {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");
  }
  void OutputFormatTestController::dispatchCollectedEvent() {
    reportEvent(APPLICATION_CHANNEL, "Event one");
  }
};

}  // namespace

TEST_CASE("ConsumeWindowsEventLog constructor works", "[create]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  REQUIRE_NOTHROW(ConsumeWindowsEventLog processor_one("one"));

  REQUIRE_NOTHROW(
    utils::Identifier uuid = utils::IdGenerator::getIdGenerator()->generate();
    ConsumeWindowsEventLog processor_two("two", uuid);
  );  // NOLINT

  REQUIRE_NOTHROW(
    auto processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  );  // NOLINT
}

TEST_CASE("ConsumeWindowsEventLog properties work with default values", "[create][properties]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConfigurableComponent>();
  LogTestController::getInstance().setTrace<ConsumeWindowsEventLog>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_controller.runSession(test_plan);

  auto properties_required_or_with_default_value = {
    ConsumeWindowsEventLog::Channel,
    ConsumeWindowsEventLog::Query,
    // ConsumeWindowsEventLog::RenderFormatXML,  // FIXME(fgerlits): not defined, does not exist in NiFi either; should be removed
    ConsumeWindowsEventLog::MaxBufferSize,
    // ConsumeWindowsEventLog::InactiveDurationToReconnect,  // FIXME(fgerlits): obsolete, see definition; should be removed
    ConsumeWindowsEventLog::IdentifierMatcher,
    ConsumeWindowsEventLog::IdentifierFunction,
    ConsumeWindowsEventLog::ResolveAsAttributes,
    ConsumeWindowsEventLog::EventHeader,
    ConsumeWindowsEventLog::OutputFormat,
    ConsumeWindowsEventLog::BatchCommitSize,
    ConsumeWindowsEventLog::BookmarkRootDirectory,  // TODO(fgerlits): obsolete, see definition; remove in a later release
    ConsumeWindowsEventLog::ProcessOldEvents
  };
  for (const core::Property& property : properties_required_or_with_default_value) {
    if (!LogTestController::getInstance().contains("property name " + property.getName() + " value ")) {
      FAIL("Property did not get queried: " << property.getName());
    }
  }

  auto properties_optional_without_default_value = {
    ConsumeWindowsEventLog::EventHeaderDelimiter
  };
  for (const core::Property& property : properties_optional_without_default_value) {
    if (!LogTestController::getInstance().contains("property name " + property.getName() + ", empty value")) {
      FAIL("Optional property did not get queried: " << property.getName());
    }
  }

  REQUIRE(LogTestController::getInstance().contains("Successfully configured CWEL"));
}

TEST_CASE("ConsumeWindowsEventLog onSchedule throws if it cannot create the bookmark", "[create][bookmark]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(processor, ConsumeWindowsEventLog::Channel.getName(), "NonexistentChannel1234981");

  REQUIRE_THROWS_AS(test_controller.runSession(test_plan), minifi::Exception);
}

TEST_CASE("ConsumeWindowsEventLog can consume new events", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog.getName(), "0");
  test_plan->setProperty(logger_processor, LogAttribute::LogPayload.getName(), "true");
  test_plan->setProperty(logger_processor, LogAttribute::MaxPayloadLineLength.getName(), "1024");

  reportEvent(APPLICATION_CHANNEL, "Event zero");

  test_controller.runSession(test_plan);
  REQUIRE(LogTestController::getInstance().contains("processed 0 Events"));
  // event zero is not reported as the bookmark is created on the first run
  // and we use the default config setting ProcessOldEvents = false
  // later runs will start with a bookmark saved in the state manager

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  SECTION("Read one event") {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    test_controller.runSession(test_plan);
    REQUIRE(LogTestController::getInstance().contains("processed 1 Events"));
    REQUIRE(LogTestController::getInstance().contains("<EventData><Data>Event one</Data></EventData>"));

    // make sure timezone attributes are present
    REQUIRE(LogTestController::getInstance().contains("key:Timezone offset value:"));
    REQUIRE(LogTestController::getInstance().contains("key:Timezone name value:"));
  }

  SECTION("Read two events") {
    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");

    test_controller.runSession(test_plan);
    REQUIRE(LogTestController::getInstance().contains("processed 2 Events"));
    REQUIRE(LogTestController::getInstance().contains("<EventData><Data>Event two</Data></EventData>"));
    REQUIRE(LogTestController::getInstance().contains("<EventData><Data>Event three</Data></EventData>"));
  }
}

TEST_CASE("ConsumeWindowsEventLog bookmarking works", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog.getName(), "0");

  reportEvent(APPLICATION_CHANNEL, "Event zero");

  test_controller.runSession(test_plan);
  REQUIRE(LogTestController::getInstance().contains("processed 0 Events"));

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  SECTION("Read in one go") {
    reportEvent(APPLICATION_CHANNEL, "Event one");
    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");

    test_controller.runSession(test_plan);
    REQUIRE(LogTestController::getInstance().contains("processed 3 Events"));
  }

  SECTION("Read in two batches") {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    test_controller.runSession(test_plan);
    REQUIRE(LogTestController::getInstance().contains("processed 1 Events"));

    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");

    test_plan->reset();
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

    test_controller.runSession(test_plan);
    REQUIRE(LogTestController::getInstance().contains("processed 2 Events"));
  }
}

TEST_CASE("ConsumeWindowsEventLog extracts some attributes by default", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog.getName(), "0");

  // 0th event, only to create a bookmark
  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    test_controller.runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  // 1st event, on Info level
  {
    reportEvent(APPLICATION_CHANNEL, "Event one: something interesting happened", EVENTLOG_INFORMATION_TYPE);

    test_controller.runSession(test_plan);

    REQUIRE(LogTestController::getInstance().contains("key:Keywords value:Classic"));
    REQUIRE(LogTestController::getInstance().contains("key:Level value:Information"));
  }

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  // 2st event, on Warning level
  {
    reportEvent(APPLICATION_CHANNEL, "Event two: something fishy happened!", EVENTLOG_WARNING_TYPE);

    test_controller.runSession(test_plan);

    REQUIRE(LogTestController::getInstance().contains("key:Keywords value:Classic"));
    REQUIRE(LogTestController::getInstance().contains("key:Level value:Warning"));
  }
}

namespace {

void outputFormatSetterTestHelper(const std::string &output_format, int expected_num_flow_files) {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), QUERY);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormat.getName(), output_format);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog.getName(), "0");

  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    test_controller.runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    test_controller.runSession(test_plan);

    REQUIRE(LogTestController::getInstance().contains("Logged " + std::to_string(expected_num_flow_files) + " flow files"));
  }
}

}  // namespace

TEST_CASE("ConsumeWindowsEventLog output format can be set", "[create][output_format]") {
  outputFormatSetterTestHelper("XML", 1);
  outputFormatSetterTestHelper("Plaintext", 1);
  outputFormatSetterTestHelper("Both", 2);

  // NOTE(fgerlits): this may be a bug, as I would expect this to throw in onSchedule(),
  // but it starts merrily, just does not write flow files in either format
  outputFormatSetterTestHelper("InvalidValue", 0);
}

// NOTE(fgerlits): I don't know how to unit test this, as my manually published events all result in an empty string if OutputFormat is Plaintext
//                 but it does seem to work, based on manual tests reading system logs
// TEST_CASE("ConsumeWindowsEventLog prints events in plain text correctly", "[onTrigger]")

TEST_CASE("ConsumeWindowsEventLog prints events in XML correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, QUERY, "XML"}.run();

  REQUIRE(event.find(R"(<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event"><System><Provider Name="Application"/>)") != std::string::npos);
  REQUIRE(event.find(R"(<EventID Qualifiers="0">14985</EventID>)") != std::string::npos);
  REQUIRE(event.find(R"(<Level>4</Level>)") != std::string::npos);
  REQUIRE(event.find(R"(<Task>0</Task>)") != std::string::npos);
  REQUIRE(event.find(R"(<Keywords>0x80000000000000</Keywords><TimeCreated SystemTime=")") != std::string::npos);
  // the timestamp (when the event was published) goes here
  REQUIRE(event.find(R"("/><EventRecordID>)") != std::string::npos);
  // the ID of the event goes here (a number)
  REQUIRE(event.find(R"(</EventRecordID>)") != std::string::npos);
  REQUIRE(event.find(R"(<Channel>Application</Channel><Computer>)") != std::string::npos);
  // the computer name goes here
  REQUIRE(event.find(R"(</Computer><Security/></System><EventData><Data>Event one</Data></EventData></Event>)") != std::string::npos);
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Simple correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, "*", "JSON", "Simple"}.run();
  // the json must be single-line
  REQUIRE(event.find('\n') == std::string::npos);
  verifyJSON(event, R"json(
    {
      "System": {
        "Provider": {
          "Name": "Application"
        },
        "Channel": "Application"
      },
      "EventData": [{
          "Type": "Data",
          "Content": "Event one",
          "Name": ""
      }]
    }
  )json");
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Flattened correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, "*", "JSON", "Flattened"}.run();
  verifyJSON(event, R"json(
    {
      "Name": "Application",
      "Channel": "Application",
      "EventData": [{
          "Type": "Data",
          "Content": "Event one",
          "Name": ""
      }]
    }
  )json");
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Raw correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, "*", "JSON", "Raw"}.run();
  verifyJSON(event, R"json(
    [
      {
        "name": "Event",
        "children": [
          {"name": "System"},
          {
            "name": "EventData",
            "children": [{
              "name": "Data",
              "text": "Event one"
            }] 
          }      
        ]
      }
    ]
  )json");
}

namespace {
void batchCommitSizeTestHelper(std::size_t num_events_read, std::size_t batch_commit_size, std::size_t expected_event_count) {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), QUERY);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormat.getName(), "XML");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::BatchCommitSize.getName(), std::to_string(batch_commit_size));

  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    test_controller.runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  auto generate_events = [](const std::size_t event_count) {
    std::vector<std::string> events;
    for (auto i = 0; i < event_count; ++i) {
      events.push_back("Event " + std::to_string(i));
    }
    return events;
  };

  for (const auto& event : generate_events(num_events_read))
    reportEvent(APPLICATION_CHANNEL, event.c_str());

  test_controller.runSession(test_plan);
  REQUIRE(LogTestController::getInstance().contains("processed " + std::to_string(expected_event_count) + " Events"));
}

}  // namespace

TEST_CASE("ConsumeWindowsEventLog batch commit size works", "[onTrigger]") {
  batchCommitSizeTestHelper(5, 1000, 5);
  batchCommitSizeTestHelper(5, 5, 5);
  batchCommitSizeTestHelper(5, 4, 4);
  batchCommitSizeTestHelper(5, 1, 1);
  batchCommitSizeTestHelper(5, 0, 5);
}
