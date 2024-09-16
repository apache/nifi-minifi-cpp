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

#include "core/ConfigurableComponentImpl.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/file/FileUtils.h"

#include "CWELTestUtils.h"
#include "unit/TestUtils.h"
#include "../wel/UniqueEvtHandle.h"
#include "../wel/JSONUtils.h"
#include "utils/Deleters.h"

using ConsumeWindowsEventLog = org::apache::nifi::minifi::processors::ConsumeWindowsEventLog;
using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;
using PutFile = org::apache::nifi::minifi::processors::PutFile;
using ConfigurableComponent = org::apache::nifi::minifi::core::ConfigurableComponent;
using IdGenerator = org::apache::nifi::minifi::utils::IdGenerator;
using unique_evt_handle = org::apache::nifi::minifi::wel::unique_evt_handle;

namespace org::apache::nifi::minifi::test {

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
  void dispatchBookmarkEvent() override {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");
  }
  void dispatchCollectedEvent() override {
    reportEvent(APPLICATION_CHANNEL, "Event one");
  }
};

class LogFileTestController : public OutputFormatTestController {
 public:
  LogFileTestController(): OutputFormatTestController("", "*", "JSON", "Simple") {
    log_file_ = createTempDirectory() / "cwel-events.evtx";
    channel_ = "SavedLog:" + log_file_.string();
  }

 protected:
  void dispatchBookmarkEvent() override {
    reportEvent(APPLICATION_CHANNEL, "Event zero from file: this is in the past");
    generateLogFile(std::wstring{APPLICATION_CHANNEL.begin(), APPLICATION_CHANNEL.end()}, log_file_);
  }
  void dispatchCollectedEvent() override {
    reportEvent(APPLICATION_CHANNEL, "Event one from file");
    std::filesystem::remove(log_file_);
    generateLogFile(std::wstring{APPLICATION_CHANNEL.begin(), APPLICATION_CHANNEL.end()}, log_file_);
    reportEvent(APPLICATION_CHANNEL, "Event not processed after log file export");
  }

  std::filesystem::path log_file_;
};

}  // namespace

TEST_CASE("ConsumeWindowsEventLog constructor works", "[create]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  REQUIRE_NOTHROW(ConsumeWindowsEventLog("one"));
  REQUIRE_NOTHROW(ConsumeWindowsEventLog("two", IdGenerator::getIdGenerator()->generate()));
  REQUIRE_NOTHROW(test_plan->addProcessor("ConsumeWindowsEventLog", "cwel"));
}

TEST_CASE("ConsumeWindowsEventLog properties work with default values", "[create][properties]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConfigurableComponent>();
  LogTestController::getInstance().setTrace<ConsumeWindowsEventLog>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  TestController::runSession(test_plan);

  CHECK(LogTestController::getInstance().contains("Successfully configured CWEL"));
}

TEST_CASE("ConsumeWindowsEventLog onSchedule throws if it cannot create the bookmark", "[create][bookmark]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(processor, ConsumeWindowsEventLog::Channel, "NonexistentChannel1234981");

  REQUIRE_THROWS_AS(test_controller.runSession(test_plan), minifi::Exception);
}

TEST_CASE("ConsumeWindowsEventLog can consume new events", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog, "0");
  test_plan->setProperty(logger_processor, LogAttribute::LogPayload, "true");
  test_plan->setProperty(logger_processor, LogAttribute::MaxPayloadLineLength, "1024");

  reportEvent(APPLICATION_CHANNEL, "Event zero");

  TestController::runSession(test_plan);
  CHECK(LogTestController::getInstance().contains("processed 0 Events"));
  // event zero is not reported as the bookmark is created on the first run
  // and we use the default config setting ProcessOldEvents = false
  // later runs will start with a bookmark saved in the state manager

  test_plan->reset();
  LogTestController::getInstance().clear();

  SECTION("Read one event") {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    TestController::runSession(test_plan);
    CHECK(LogTestController::getInstance().contains("processed 1 Events"));
    CHECK(LogTestController::getInstance().contains("<EventData><Data>Event one</Data></EventData>"));

    // make sure timezone attributes are present
    CHECK(LogTestController::getInstance().contains("key:timezone.offset value:"));
    CHECK(LogTestController::getInstance().contains("key:timezone.name value:"));
  }

  SECTION("Read three events") {
    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");
    reportEvent(APPLICATION_CHANNEL, "%%1844");  // %%1844 expands to System

    TestController::runSession(test_plan);
    CHECK(LogTestController::getInstance().contains("processed 3 Events"));
    CHECK(LogTestController::getInstance().contains("<EventData><Data>Event two</Data></EventData>"));
    CHECK(LogTestController::getInstance().contains("<EventData><Data>Event three</Data></EventData>"));
    CHECK(LogTestController::getInstance().contains("<EventData><Data>System</Data></EventData>"));
  }
}

TEST_CASE("ConsumeWindowsEventLog bookmarking works", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog, "0");

  reportEvent(APPLICATION_CHANNEL, "Event zero");

  TestController::runSession(test_plan);
  CHECK(LogTestController::getInstance().contains("processed 0 Events"));

  test_plan->reset();
  LogTestController::getInstance().clear();

  SECTION("Read in one go") {
    reportEvent(APPLICATION_CHANNEL, "Event one");
    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");

    TestController::runSession(test_plan);
    CHECK(LogTestController::getInstance().contains("processed 3 Events"));
  }

  SECTION("Read in two batches") {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    TestController::runSession(test_plan);
    CHECK(LogTestController::getInstance().contains("processed 1 Events"));

    reportEvent(APPLICATION_CHANNEL, "Event two");
    reportEvent(APPLICATION_CHANNEL, "Event three");

    test_plan->reset();
    LogTestController::getInstance().clear();

    TestController::runSession(test_plan);
    CHECK(LogTestController::getInstance().contains("processed 2 Events"));
  }
}

TEST_CASE("ConsumeWindowsEventLog extracts some attributes by default", "[onTrigger]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, QUERY);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog, "0");

  SECTION("XML output") {
    REQUIRE(test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, "XML"));
  }

  SECTION("Json output") {
    REQUIRE(test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, "JSON"));
  }

  SECTION("Plaintext output") {
    REQUIRE(test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, "Plaintext"));
  }

  // 0th event, only to create a bookmark
  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    TestController::runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().clear();

  // 1st event, on Info level
  {
    reportEvent(APPLICATION_CHANNEL, "Event one: something interesting happened", EVENTLOG_INFORMATION_TYPE);

    TestController::runSession(test_plan);

    CHECK(LogTestController::getInstance().contains("key:Keywords value:Classic"));
    CHECK(LogTestController::getInstance().contains("key:Level value:Information"));
  }

  test_plan->reset();
  LogTestController::getInstance().clear();

  // 2st event, on Warning level
  {
    reportEvent(APPLICATION_CHANNEL, "Event two: something fishy happened!", EVENTLOG_WARNING_TYPE);

    TestController::runSession(test_plan);

    CHECK(LogTestController::getInstance().contains("key:Keywords value:Classic"));
    CHECK(LogTestController::getInstance().contains("key:Level value:Warning"));
  }
}

namespace {

void outputFormatSetterTestHelper(const std::string &output_format, int expected_num_flow_files) {
  TestController test_controller;
  LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
  LogTestController::getInstance().setDebug<LogAttribute>();
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();

  auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, QUERY);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, output_format);

  auto logger_processor = test_plan->addProcessor("LogAttribute", "logger", Success, true);
  test_plan->setProperty(logger_processor, LogAttribute::FlowFilesToLog, "0");

  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    TestController::runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().clear();

  {
    reportEvent(APPLICATION_CHANNEL, "Event one");

    TestController::runSession(test_plan);

    CHECK(LogTestController::getInstance().contains("Logged " + std::to_string(expected_num_flow_files) + " flow files"));
  }
}

}  // namespace

TEST_CASE("ConsumeWindowsEventLog output format can be set", "[create][output_format]") {
  outputFormatSetterTestHelper("XML", 1);
  outputFormatSetterTestHelper("Plaintext", 1);
  outputFormatSetterTestHelper("Both", 2);
}

TEST_CASE("ConsumeWindowsEventLog prints events in plain text correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, QUERY, "Plaintext"}.run();
  CHECK(!event.empty());
  CHECK(event.find(R"(Log Name:      Application)") != std::string::npos);
  CHECK(event.find(R"(Source:        Application)") != std::string::npos);
  CHECK(event.find(R"(Date:          )") != std::string::npos);
  CHECK(event.find(R"(Record ID:     )") != std::string::npos);
  CHECK(event.find(R"(Event ID:      14985)") != std::string::npos);
  CHECK((event.find(R"(Task Category: N/A)") != std::string::npos || event.find(R"(Task Category: None)") != std::string::npos));
  CHECK(event.find(R"(Level:         Information)") != std::string::npos);
  CHECK(event.find(R"(Keywords:      Classic)") != std::string::npos);
  CHECK(event.find(R"(User:          N/A)") != std::string::npos);
  CHECK(event.find(R"(Computer:      )") != std::string::npos);
  CHECK(event.find(R"(EventType:     4)") != std::string::npos);
  CHECK(event.find(R"(Error:         The message resource is present but the message was not found in the message table.)") != std::string::npos);
}

TEST_CASE("ConsumeWindowsEventLog prints events in XML correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, QUERY, "XML"}.run();

  CHECK(event.find(R"(<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event"><System><Provider Name="Application"/>)") != std::string::npos);
  CHECK(event.find(R"(<EventID Qualifiers="0">14985</EventID>)") != std::string::npos);
  CHECK(event.find(R"(<Level>4</Level>)") != std::string::npos);
  CHECK(event.find(R"(<Task>0</Task>)") != std::string::npos);
  CHECK(event.find(R"(<Keywords>0x80000000000000</Keywords><TimeCreated SystemTime=")") != std::string::npos);
  // the timestamp (when the event was published) goes here
  CHECK(event.find(R"("/><EventRecordID>)") != std::string::npos);
  // the ID of the event goes here (a number)
  CHECK(event.find(R"(</EventRecordID>)") != std::string::npos);
  CHECK(event.find(R"(<Channel>Application</Channel><Computer>)") != std::string::npos);
  // the computer name goes here
  CHECK(event.find(R"(</Computer><Security/></System><EventData><Data>Event one</Data></EventData></Event>)") != std::string::npos);
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Simple correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, "*", "JSON", "Simple"}.run();
  // the json must be single-line
  CHECK(event.find('\n') == std::string::npos);
  utils::verifyJSON(event, R"json(
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
  utils::verifyJSON(event, R"json(
    {
      "Name": "Application",
      "Channel": "Application",
      "EventData": "Event one"
    }
  )json");
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Raw correctly", "[onTrigger]") {
  std::string event = SimpleFormatTestController{APPLICATION_CHANNEL, "*", "JSON", "Raw"}.run();
  utils::verifyJSON(event, R"json(
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
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, APPLICATION_CHANNEL);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, QUERY);
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, "XML");
  test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::BatchCommitSize, std::to_string(batch_commit_size));

  {
    reportEvent(APPLICATION_CHANNEL, "Event zero: this is in the past");

    TestController::runSession(test_plan);
  }

  test_plan->reset();
  LogTestController::getInstance().clear();

  auto generate_events = [](const std::size_t event_count) {
    std::vector<std::string> events;
    for (auto i = 0; i < event_count; ++i) {
      events.push_back("Event " + std::to_string(i));
    }
    return events;
  };

  for (const auto& event : generate_events(num_events_read))
    reportEvent(APPLICATION_CHANNEL, event.c_str());

  TestController::runSession(test_plan);
  CHECK(LogTestController::getInstance().contains("processed " + std::to_string(expected_event_count) + " Events"));
}

}  // namespace

TEST_CASE("ConsumeWindowsEventLog batch commit size works", "[onTrigger]") {
  batchCommitSizeTestHelper(5, 1000, 5);
  batchCommitSizeTestHelper(5, 5, 5);
  batchCommitSizeTestHelper(5, 4, 4);
  batchCommitSizeTestHelper(5, 1, 1);
  batchCommitSizeTestHelper(5, 0, 5);
}

TEST_CASE("ConsumeWindowsEventLog Simple JSON works with UserData", "[cwel][json][userdata]") {
  using org::apache::nifi::minifi::wel::jsonToString;
  using org::apache::nifi::minifi::wel::toSimpleJSON;
  using org::apache::nifi::minifi::wel::toFlattenedJSON;
  const auto event_xml = R"(
<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
  <System>
    <Provider Name="Microsoft-Windows-AppLocker" Guid="CBDA4DBF-8D5D-4F69-9578-BE14AA540D22">
    </Provider>
    <EventID>8002</EventID>
    <Version>0</Version>
    <Level>4</Level>
    <Task>0</Task>
    <Opcode>0</Opcode>
    <Keywords>0x8000000000000000</Keywords>
    <TimeCreated SystemTime="2023-02-06T16:58:09.008534Z">
    </TimeCreated>
    <EventRecordID>46</EventRecordID>
    <Correlation>
    </Correlation>
    <Execution ProcessID="1234" ThreadID="1235">
    </Execution>
    <Channel>Microsoft-Windows-AppLocker/EXE and DLL</Channel>
    <Computer>example.local</Computer>
    <Security UserID="S-1-1-0">
    </Security>
  </System>
  <UserData>
    <RuleAndFileData xmlns="http://schemas.microsoft.com/schemas/event/Microsoft.Windows/1.0.0.0">
      <PolicyNameLength>3</PolicyNameLength>
      <PolicyName>EXE</PolicyName>
      <RuleNameLength>9</RuleNameLength>
      <RuleName>All files</RuleName>
      <RuleSddlLength>48</RuleSddlLength>
      <RuleSddl>D:(XA;;FX;;;S-1-1-0;(APPID://PATH Contains &quot;*&quot;))</RuleSddl>
      <TargetUser>S-1-1-0</TargetUser>
      <TargetProcessId>1234</TargetProcessId>
      <FilePathLength>22</FilePathLength>
      <FilePath>%SYSTEM32%\CSCRIPT.EXE</FilePath>
      <FileHashLength>0</FileHashLength>
      <FileHash></FileHash>
      <FqbnLength>1</FqbnLength>
      <Fqbn>-</Fqbn>
      <Parent foo="bar"><Child/></Parent>
      <Leaf foo="bar"></Leaf>
      <AltLeaf foo="bar"/>
    </RuleAndFileData>
  </UserData>
</Event>
)";
  pugi::xml_document doc;
  REQUIRE(doc.load_string(event_xml));
  SECTION("simple") {
    const auto simple_json = jsonToString(toSimpleJSON(doc));
    const auto expected_json = R"json({"System":{"Provider":{"Name":"Microsoft-Windows-AppLocker","Guid":"CBDA4DBF-8D5D-4F69-9578-BE14AA540D22"},"EventID":"8002","Version":"0","Level":"4","Task":"0","Opcode":"0","Keywords":"0x8000000000000000","TimeCreated":{"SystemTime":"2023-02-06T16:58:09.008534Z"},"EventRecordID":"46","Correlation":{},"Execution":{"ProcessID":"1234","ThreadID":"1235"},"Channel":"Microsoft-Windows-AppLocker/EXE and DLL","Computer":"example.local","Security":{"UserID":"S-1-1-0"}},"EventData":[],"UserData":{"RuleAndFileData":{"PolicyNameLength":"3","PolicyName":"EXE","RuleNameLength":"9","RuleName":"All files","RuleSddlLength":"48","RuleSddl":"D:(XA;;FX;;;S-1-1-0;(APPID://PATH Contains \"*\"))","TargetUser":"S-1-1-0","TargetProcessId":"1234","FilePathLength":"22","FilePath":"%SYSTEM32%\\CSCRIPT.EXE","FileHashLength":"0","FileHash":"","FqbnLength":"1","Fqbn":"-","Parent":{"foo":"bar","Child":""},"Leaf":"","AltLeaf":""}}})json";  // NOLINT(whitespace/line_length): long raw string, impractical to split
    CHECK(expected_json == simple_json);
  }
  SECTION("flattened") {
    const auto flattened_json = jsonToString(toFlattenedJSON(doc));
    const auto expected_json = R"json({"Name":"Microsoft-Windows-AppLocker","Guid":"CBDA4DBF-8D5D-4F69-9578-BE14AA540D22","EventID":"8002","Version":"0","Level":"4","Task":"0","Opcode":"0","Keywords":"0x8000000000000000","SystemTime":"2023-02-06T16:58:09.008534Z","EventRecordID":"46","ProcessID":"1234","ThreadID":"1235","Channel":"Microsoft-Windows-AppLocker/EXE and DLL","Computer":"example.local","UserID":"S-1-1-0","UserData.RuleAndFileData.PolicyNameLength":"3","UserData.RuleAndFileData.PolicyName":"EXE","UserData.RuleAndFileData.RuleNameLength":"9","UserData.RuleAndFileData.RuleName":"All files","UserData.RuleAndFileData.RuleSddlLength":"48","UserData.RuleAndFileData.RuleSddl":"D:(XA;;FX;;;S-1-1-0;(APPID://PATH Contains \"*\"))","UserData.RuleAndFileData.TargetUser":"S-1-1-0","UserData.RuleAndFileData.TargetProcessId":"1234","UserData.RuleAndFileData.FilePathLength":"22","UserData.RuleAndFileData.FilePath":"%SYSTEM32%\\CSCRIPT.EXE","UserData.RuleAndFileData.FileHashLength":"0","UserData.RuleAndFileData.FileHash":"","UserData.RuleAndFileData.FqbnLength":"1","UserData.RuleAndFileData.Fqbn":"-","UserData.RuleAndFileData.Parent.foo":"bar","UserData.RuleAndFileData.Parent.Child":"","UserData.RuleAndFileData.Leaf":"","UserData.RuleAndFileData.AltLeaf":""})json";  // NOLINT(whitespace/line_length): long raw string, impractical to split
    CHECK(expected_json == flattened_json);
  }
}

TEST_CASE("ConsumeWindowsEventLog can process events from a log file", "[cwel][logfile]") {
  std::string event = LogFileTestController{}.run();
  utils::verifyJSON(event, R"json(
    {
      "System": {
        "Provider": {
          "Name": "Application"
        },
        "Channel": "Application"
      },
      "EventData": [{
          "Type": "Data",
          "Content": "Event one from file",
          "Name": ""
      }]
    }
  )json");
}

}  // namespace org::apache::nifi::minifi::test
