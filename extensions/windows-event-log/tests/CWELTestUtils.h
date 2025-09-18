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

#pragma once

#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ConsumeWindowsEventLog.h"
#include "processors/PutFile.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/StringUtils.h"
#include "unit/TestUtils.h"
#include "utils/UnicodeConversion.h"
#include "utils/file/FileUtils.h"
#include "minifi-cpp/utils/gsl.h"

core::Relationship Success{"success", "Everything is fine"};

using ConsumeWindowsEventLog = org::apache::nifi::minifi::processors::ConsumeWindowsEventLog;
using PutFile = org::apache::nifi::minifi::processors::PutFile;

class OutputFormatTestController : public TestController {
 public:
  OutputFormatTestController(std::string channel, std::string query, std::string output_format, std::optional<std::string> json_format = {})
    : channel_(std::move(channel)),
      query_(std::move(query)),
      output_format_(std::move(output_format)),
      json_format_(std::move(json_format)) {}

  std::string run() {
    LogTestController::getInstance().setTrace<ConsumeWindowsEventLog>();
    LogTestController::getInstance().setDebug<PutFile>();
    std::shared_ptr<TestPlan> test_plan = createPlan();

    auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel, channel_);
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query, query_);
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormatProperty, output_format_);
    if (json_format_) {
      test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::JsonFormatProperty, json_format_.value());
    }

    auto dir = createTempDirectory();

    auto put_file = test_plan->addProcessor("PutFile", "putFile", Success, true);
    test_plan->setProperty(put_file, PutFile::Directory, dir.string());

    {
      dispatchBookmarkEvent();

      runSession(test_plan);
    }

    test_plan->reset();
    LogTestController::getInstance().clear();


    {
      dispatchCollectedEvent();

      runSession(test_plan);

      auto files = utils::file::list_dir_all(dir, LogTestController::getInstance().getLogger<LogTestController>(), false);
      REQUIRE(files.size() == 1);

      std::ifstream file{files[0].first / files[0].second};
      return {std::istreambuf_iterator<char>{file}, {}};
    }
  }

 protected:
  virtual void dispatchBookmarkEvent() = 0;
  virtual void dispatchCollectedEvent() = 0;

  std::string channel_;
  std::string query_;
  std::string output_format_;
  std::optional<std::string> json_format_;
};

void generateLogFile(const std::wstring& channel, const std::filesystem::path& path) {
  const auto channel_as_string = utils::to_string(channel);
  HANDLE event_log = OpenEventLog(NULL, channel_as_string.c_str());
  auto guard = gsl::finally([&] {CloseEventLog(event_log);});

  if (!EvtExportLog(NULL, channel.c_str(), L"*", path.wstring().c_str(), EvtExportLogChannelPath)) {
    throw std::system_error{gsl::narrow<int>(GetLastError()), std::system_category(), "Failed to export logs"};
  }
}
