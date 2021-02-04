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

#include "ConsumeWindowsEventLog.h"
#include "processors/PutFile.h"
#include "TestBase.h"
#include "utils/TestUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/OptionalUtils.h"

core::Relationship Success{"success", "Everything is fine"};

using ConsumeWindowsEventLog = org::apache::nifi::minifi::processors::ConsumeWindowsEventLog;
using PutFile = org::apache::nifi::minifi::processors::PutFile;

class OutputFormatTestController : public TestController {
 public:
  OutputFormatTestController(std::string channel, std::string query, std::string output_format, utils::optional<std::string> json_format = {})
    : channel_(std::move(channel)),
      query_(std::move(query)),
      output_format_(std::move(output_format)),
      json_format_(std::move(json_format)) {}

  std::string run() {
    LogTestController::getInstance().setDebug<ConsumeWindowsEventLog>();
    LogTestController::getInstance().setDebug<PutFile>();
    std::shared_ptr<TestPlan> test_plan = createPlan();

    auto cwel_processor = test_plan->addProcessor("ConsumeWindowsEventLog", "cwel");
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Channel.getName(), channel_);
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::Query.getName(), query_);
    test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::OutputFormat.getName(), output_format_);
    if (json_format_) {
      test_plan->setProperty(cwel_processor, ConsumeWindowsEventLog::JSONFormat.getName(), json_format_.value());
    }

    auto dir = utils::createTempDir(this);

    auto put_file = test_plan->addProcessor("PutFile", "putFile", Success, true);
    test_plan->setProperty(put_file, PutFile::Directory.getName(), dir);

    {
      dispatchBookmarkEvent();

      runSession(test_plan);
    }

    test_plan->reset();
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);


    {
      dispatchCollectedEvent();

      runSession(test_plan);

      auto files = utils::file::list_dir_all(dir, LogTestController::getInstance().getLogger<LogTestController>(), false);
      REQUIRE(files.size() == 1);

      std::ifstream file{utils::file::concat_path(files[0].first, files[0].second)};
      return {std::istreambuf_iterator<char>{file}, {}};
    }
  }

 protected:
  virtual void dispatchBookmarkEvent() = 0;
  virtual void dispatchCollectedEvent() = 0;

  std::string channel_;
  std::string query_;
  std::string output_format_;
  utils::optional<std::string> json_format_;
};
