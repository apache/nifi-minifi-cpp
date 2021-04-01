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

#undef NDEBUG

#include "ConsumeWindowsEventLog.h"

#include "core/ConfigurableComponent.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "TestBase.h"
#include "utils/TestUtils.h"
#include "utils/file/FileUtils.h"
#include "rapidjson/document.h"
#include "wel/UniqueEvtHandle.h"
#include "IntegrationTestUtils.h"

#include "CWELTestUtils.h"
#include "Utils.h"

// generated from the manifest file "custom-provider/unit-test-provider.man"
// using the command "mc -um unit-test-provider.man"
#include "custom-provider/unit-test-provider.h"

namespace {

struct CustomEventData {
  std::wstring first;
  std::wstring second;
  std::wstring third;
  int binary_length;
  const unsigned char* binary_data;
};

const std::string CUSTOM_PROVIDER_NAME = "minifi_unit_test_provider";
const std::string CUSTOM_CHANNEL = CUSTOM_PROVIDER_NAME + "/Log";

bool dispatchCustomEvent(const CustomEventData& event) {
  static auto provider_initialized = EventRegisterminifi_unit_test_provider();
  REQUIRE(provider_initialized == ERROR_SUCCESS);

  auto result = EventWriteCustomEvent(
    event.first.c_str(),
    event.second.c_str(),
    event.third.c_str(),
    event.binary_length,
    event.binary_data
  );
  return result == ERROR_SUCCESS;
}

using org::apache::nifi::minifi::wel::unique_evt_handle;

bool advanceBookmark(const unique_evt_handle& hBookmark, const std::string& channel, const std::string& query, bool advance_to_last = false) {
  const auto hEventResults = unique_evt_handle{ EvtQuery(0, std::wstring{channel.begin(), channel.end()}.c_str(), std::wstring{query.begin(), query.end()}.c_str(), EvtQueryChannelPath) };
  if (!hEventResults) {
    return false;
  }

  if (advance_to_last) {
    if (!EvtSeek(hEventResults.get(), 0, 0, 0, EvtSeekRelativeToLast)) {
      return false;
    }
  } else {
    if (!EvtSeek(hEventResults.get(), 1, hBookmark.get(), 0, EvtSeekRelativeToBookmark)) {
      return false;
    }
  }

  const unique_evt_handle hEvent = [&hEventResults] {
    DWORD dwReturned{};
    EVT_HANDLE hEvent{ nullptr };
    EvtNext(hEventResults.get(), 1, &hEvent, INFINITE, 0, &dwReturned);
    return unique_evt_handle{ hEvent };
  }();

  if (!hEvent) {
    return false;
  }

  REQUIRE(EvtUpdateBookmark(hBookmark.get(), hEvent.get()));

  return true;
}

class CustomProviderController : public OutputFormatTestController {
 public:
  CustomProviderController(std::string format, std::string json_format) : OutputFormatTestController(CUSTOM_CHANNEL, "*", std::move(format), std::move(json_format)) {
    bookmark_.reset(EvtCreateBookmark(0));
    advanceBookmark(bookmark_, channel_, query_, true);
    REQUIRE(bookmark_);
  }

 protected:
  void dispatchBookmarkEvent() override {
    auto binary = reinterpret_cast<const unsigned char*>("\x0c\x10");
    REQUIRE(dispatchCustomEvent({L"Bookmark", L"Second", L"Third", 2, binary}));
    REQUIRE(checkNewEventAvailable());
  }
  void dispatchCollectedEvent() override {
    auto binary = reinterpret_cast<const unsigned char*>("\x09\x01");
    REQUIRE(dispatchCustomEvent({L"Actual event", L"Second", L"Third", 2, binary}));
    REQUIRE(checkNewEventAvailable());
  }

 private:
  bool checkNewEventAvailable() {
    return org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime(std::chrono::seconds{5}, [&] {
      return advanceBookmark(bookmark_, channel_, query_);
    });
  }
  unique_evt_handle bookmark_;
};

const std::string EVENT_DATA_JSON = R"(
  [{
    "Type": "Data",
    "Content": "Actual event",
    "Name": "param1"
  }, {
    "Type": "Data",
    "Content": "Second",
    "Name": "param2"
  }, {
    "Type": "Data",
    "Content": "Third",
    "Name": "Channel"
  }, {
    "Type": "Binary",
    "Content": "0901",
    "Name": ""
  }]
)";

}  // namespace

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Simple correctly custom provider", "[onTrigger]") {
  std::string event = CustomProviderController{"JSON", "Simple"}.run();
  verifyJSON(event, R"(
    {
      "System": {
        "Provider": {
          "Name": ")" + CUSTOM_PROVIDER_NAME + R"("
        },
        "Channel": ")" + CUSTOM_CHANNEL + R"("
      },
      "EventData": )" + EVENT_DATA_JSON + R"(
    }
  )");
}

TEST_CASE("ConsumeWindowsEventLog prints events in JSON::Flattened correctly custom provider", "[onTrigger]") {
  std::string event = CustomProviderController{"JSON", "Flattened"}.run();
  verifyJSON(event, R"(
    {
      "Name": ")" + CUSTOM_PROVIDER_NAME + R"(",
      "Channel": ")" + CUSTOM_CHANNEL /* Channel is not overwritten by data named "Channel" */ + R"(",
      "EventData": )" + EVENT_DATA_JSON /* EventData is not discarded */ + R"(,
      "param1": "Actual event",
      "param2": "Second"
    }
  )");
}
