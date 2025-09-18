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

#include <memory>
#include <regex>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/utils/gsl.h"
#include "wel/Bookmark.h"
#include "wel/UniqueEvtHandle.h"
#include "CWELTestUtils.h"

using Bookmark = org::apache::nifi::minifi::wel::Bookmark;
using unique_evt_handle = org::apache::nifi::minifi::wel::unique_evt_handle;
using IdGenerator = org::apache::nifi::minifi::utils::IdGenerator;

namespace {

const std::wstring APPLICATION_CHANNEL = L"Application";

constexpr DWORD BOOKMARK_TESTS_OPCODE = 10368;  // random opcode hopefully won't clash with something important

constexpr DWORD EVT_NEXT_TIMEOUT_MS = 100;

std::unique_ptr<Bookmark> createBookmark(TestPlan &test_plan,
                                         const std::wstring &channel,
                                         const utils::Identifier &uuid,
                                         core::StateManager* state_manager) {
  const auto logger = test_plan.getLogger();

  return std::make_unique<Bookmark>(minifi::wel::EventPath{channel}, L"*", "", uuid, false, state_manager, logger);
}

void reportEvent(const std::wstring& channel, const char* message) {
  auto event_source = RegisterEventSourceW(nullptr, channel.c_str());
  auto event_source_deleter = gsl::finally([&event_source](){ DeregisterEventSource(event_source); });
  ReportEventA(event_source, EVENTLOG_INFORMATION_TYPE, 0,
               BOOKMARK_TESTS_OPCODE, nullptr, 1, 0, &message, nullptr);
}

std::wstring bookmarkHandleAsXml(EVT_HANDLE event) {
  REQUIRE(event);
  constexpr std::size_t BUFFER_SIZE = 1024;
  std::array<wchar_t, BUFFER_SIZE> buffer = {};
  DWORD buffer_used;
  DWORD property_count;
  if (!EvtRender(nullptr, event, EvtRenderBookmark, gsl::narrow<DWORD>(buffer.size()), buffer.data(), &buffer_used, &property_count)) {
    FAIL("EvtRender() failed; error code: " << GetLastError());
  }
  return std::wstring{buffer.data()};
}

std::wstring bookmarkAsXml(const std::unique_ptr<Bookmark>& bookmark) {
  REQUIRE(bookmark);
  REQUIRE(bookmark->isValid());
  return bookmarkHandleAsXml(bookmark->getBookmarkHandleFromXML());
}

unique_evt_handle queryEvents() {
  std::wstring query = L"Event/System/EventID=" + std::to_wstring(BOOKMARK_TESTS_OPCODE);
  unique_evt_handle results{EvtQuery(NULL, APPLICATION_CHANNEL.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection)};
  if (!results) {
    FAIL("EvtQuery() failed; error code: " << GetLastError());
  }
  return results;
}

unique_evt_handle getFirstEventFromResults(const unique_evt_handle& results) {
  REQUIRE(results);
  EVT_HANDLE event_raw_handle = 0;
  DWORD num_results_found = 0;
  if (!EvtNext(results.get(), 1, &event_raw_handle, EVT_NEXT_TIMEOUT_MS, 0, &num_results_found)) {
    FAIL("EvtNext() failed; error code: " << GetLastError());
  }
  unique_evt_handle event{event_raw_handle};
  REQUIRE(event);
  REQUIRE(num_results_found == 1);
  return event;
}

}  // namespace

TEST_CASE("Bookmark constructor works", "[create]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  reportEvent(APPLICATION_CHANNEL, "Publish an event to make sure the event log is not empty");


  const utils::Identifier uuid = IdGenerator::getIdGenerator()->generate();
  auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
  std::unique_ptr<Bookmark> bookmark = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
  REQUIRE(bookmark);
  REQUIRE(bookmark->isValid());

  std::wregex pattern{L"<BookmarkList Direction='backward'>\r\n"
                      L"  <Bookmark Channel='Application' RecordId='\\d+' IsCurrent='true'/>\r\n"
                      L"</BookmarkList>"};
  REQUIRE(std::regex_match(bookmarkAsXml(bookmark), pattern));
}

TEST_CASE("Bookmark constructor works for log file path", "[create]") {
  TestController test_controller;
  std::filesystem::path log_file = test_controller.createTempDirectory() / "events.evtx";
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  reportEvent(APPLICATION_CHANNEL, "Publish an event to make sure the event log is not empty");
  generateLogFile(APPLICATION_CHANNEL, log_file);


  const utils::Identifier uuid = IdGenerator::getIdGenerator()->generate();
  auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
  std::unique_ptr<Bookmark> bookmark = createBookmark(*test_plan, L"SavedLog:" + log_file.wstring(), uuid, state_manager.get());
  REQUIRE(bookmark);
  REQUIRE(bookmark->isValid());

  std::string log_file_str = log_file.string();
  utils::string::replaceAll(log_file_str, "\\", "\\\\");

  std::wstring pattern{L"<BookmarkList Direction='backward'>\r\n"};
  pattern += L"  <Bookmark Channel='" + std::wstring(log_file_str.begin(), log_file_str.end()) + L"' RecordId='\\d+' IsCurrent='true'/>\r\n";
  pattern += L"</BookmarkList>";

  REQUIRE(std::regex_match(bookmarkAsXml(bookmark), std::wregex{pattern}));
}

TEST_CASE("Bookmark is restored from the state", "[create][state]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  utils::Identifier uuid = IdGenerator::getIdGenerator()->generate();
  auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
  std::unique_ptr<Bookmark> bookmark_before = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
  std::wstring bookmark_xml_before = bookmarkAsXml(bookmark_before);

  reportEvent(APPLICATION_CHANNEL, "Something interesting happened");

  // same uuid, so the state manager is the same as before
  std::unique_ptr<Bookmark> bookmark_after = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
  std::wstring bookmark_xml_after = bookmarkAsXml(bookmark_after);

  REQUIRE(bookmark_xml_before == bookmark_xml_after);
}

TEST_CASE("Bookmark created after a new event is different", "[create][state]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  utils::Identifier uuid_one = IdGenerator::getIdGenerator()->generate();
  auto state_manager_one = test_plan->getStateStorage()->getStateManager(uuid_one);
  std::unique_ptr<Bookmark> bookmark_before = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_one, state_manager_one.get());

  reportEvent(APPLICATION_CHANNEL, "Something interesting happened");

  utils::Identifier uuid_two = IdGenerator::getIdGenerator()->generate();
  // different uuid, so we get a new, empty, state manager
  auto state_manager_two = test_plan->getStateStorage()->getStateManager(uuid_two);
  std::unique_ptr<Bookmark> bookmark_after = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_two, state_manager_two.get());

  REQUIRE(bookmarkAsXml(bookmark_before) != bookmarkAsXml(bookmark_after));
}

TEST_CASE("Bookmark::getBookmarkHandleFromXML() returns the same event from a copy", "[handle_from_xml]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  utils::Identifier uuid = IdGenerator::getIdGenerator()->generate();
  auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
  std::unique_ptr<Bookmark> bookmark_one = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
  std::unique_ptr<Bookmark> bookmark_two = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());

  EVT_HANDLE bookmark_handle_one = bookmark_one->getBookmarkHandleFromXML();
  EVT_HANDLE bookmark_handle_two = bookmark_two->getBookmarkHandleFromXML();
  REQUIRE(bookmarkHandleAsXml(bookmark_handle_one) == bookmarkHandleAsXml(bookmark_handle_two));
}

TEST_CASE("Bookmark::getBookmarkHandleFromXML() returns a different event after the XML is changed", "[handle_from_xml]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  GIVEN("We have two different bookmarks") {
    const auto uuid = IdGenerator::getIdGenerator()->generate();
    auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
    std::unique_ptr<Bookmark> bookmark_one = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
    std::wstring bookmark_one_xml = bookmarkAsXml(bookmark_one);

    reportEvent(APPLICATION_CHANNEL, "Something interesting happened");

    const utils::Identifier uuid_two = IdGenerator::getIdGenerator()->generate();
    auto state_manager_two = test_plan->getStateStorage()->getStateManager(uuid_two);
    std::unique_ptr<Bookmark> bookmark_two = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_two, state_manager_two.get());
    std::wstring bookmark_two_xml = bookmarkAsXml(bookmark_two);

    REQUIRE(bookmark_one_xml != bookmark_two_xml);

    WHEN("we set the XML of the first bookmark equal to the XML of the second bookmark") {
      bookmark_one->saveBookmarkXml(bookmark_two_xml);

      THEN("getBookmarkHandleFromXML() will return the updated handle") {
        EVT_HANDLE bookmark_one_handle = bookmark_one->getBookmarkHandleFromXML();
        REQUIRE(bookmarkHandleAsXml(bookmark_one_handle) == bookmark_two_xml);
      }

      THEN("... and the two bookmarks are now equal.") {
        REQUIRE(bookmarkAsXml(bookmark_one) == bookmarkAsXml(bookmark_two));
      }
    }
  }
}

TEST_CASE("Bookmark::getNewBookmarkXml() updates the bookmark", "[add_event]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  const utils::Identifier uuid = IdGenerator::getIdGenerator()->generate();
  auto state_manager = test_plan->getStateStorage()->getStateManager(uuid);
  std::unique_ptr<Bookmark> bookmark = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid, state_manager.get());
  std::wstring bookmark_xml_before = bookmarkAsXml(bookmark);

  reportEvent(APPLICATION_CHANNEL, "Something interesting happened");

  unique_evt_handle results = queryEvents();
  unique_evt_handle event = getFirstEventFromResults(results);

  auto bookmark_xml_after = bookmark->getNewBookmarkXml(event.get());

  REQUIRE(bookmark_xml_after);
  CHECK(bookmark_xml_before != *bookmark_xml_after);
}

TEST_CASE("Bookmark::saveBookmarkXml() updates the bookmark and saves it to the state manager", "[save_bookmark][state]") {
  TestController test_controller;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan();
  LogTestController::getInstance().setTrace<TestPlan>();

  GIVEN("We have two different bookmarks with two different state managers") {
    utils::Identifier uuid_one = IdGenerator::getIdGenerator()->generate();
    auto state_manager_one = test_plan->getStateStorage()->getStateManager(uuid_one);
    std::unique_ptr<Bookmark> bookmark_one = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_one, state_manager_one.get());

    reportEvent(APPLICATION_CHANNEL, "Something interesting happened");

    utils::Identifier uuid_two = IdGenerator::getIdGenerator()->generate();
    auto state_manager_two = test_plan->getStateStorage()->getStateManager(uuid_two);
    std::unique_ptr<Bookmark> bookmark_two = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_two, state_manager_two.get());

    REQUIRE(bookmarkAsXml(bookmark_one) != bookmarkAsXml(bookmark_two));

    WHEN("we create a new bookmark with state manager one") {
      std::unique_ptr<Bookmark> bookmark_one_same = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_one, state_manager_one.get());

      THEN("it will be the same as bookmark one.") {
        REQUIRE(bookmarkAsXml(bookmark_one_same) == bookmarkAsXml(bookmark_one));
      }
    }

    WHEN("saveBookmarkXml() is called on bookmark one with the XML of bookmark two, "
         "and then we create a new bookmark with state manager one") {
      bookmark_one->saveBookmarkXml(bookmarkAsXml(bookmark_two));
      std::unique_ptr<Bookmark> bookmark_one_different = createBookmark(*test_plan, APPLICATION_CHANNEL, uuid_one, state_manager_one.get());

      THEN("it will be the same as bookmark two.") {
        REQUIRE(bookmarkAsXml(bookmark_one_different) == bookmarkAsXml(bookmark_two));
      }
    }
  }
}
