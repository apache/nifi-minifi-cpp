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
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "TestBase.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"
#include "ProcFsMonitor.h"
#include "rapidjson/filereadstream.h"

using org::apache::nifi::minifi::processors::PutFile;
using org::apache::nifi::minifi::procfs::processors::ProcFsMonitor;

class ProcFsMonitorTester {
 public:
  ProcFsMonitorTester() {
    LogTestController::getInstance().setTrace<TestPlan>();
    dir_ = test_controller_.createTempDirectory();
    plan_ = test_controller_.createPlan();
    procfs_monitor_ = plan_->addProcessor("ProcFsMonitor", "procfsmonitor");
    putfile_ = plan_->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);
    plan_->setProperty(putfile_, PutFile::Directory.getName(), dir_);
  }

  void setProcFsMonitorProperty(const core::Property& property, const std::string& value) {
    plan_->setProperty(procfs_monitor_, property.getName(), value);
  }

  bool runWithRetries(std::function<bool()>&& assertions, uint32_t max_tries = 10) {
    for (uint32_t tries = 0; tries < max_tries; ++tries) {
      test_controller_.runSession(plan_);
      if (assertions()) {
        return true;
      }
      plan_->reset();
      LogTestController::getInstance().reset();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
  }

  TestController test_controller_;
  std::string dir_;
  std::shared_ptr<TestPlan> plan_;
  std::shared_ptr<core::Processor> procfs_monitor_;
  std::shared_ptr<core::Processor> putfile_;
};

TEST_CASE("ProcFsMonitorAbsoluteTest", "[procfsmonitorabsolutetest]") {
  ProcFsMonitorTester tester;
  tester.setProcFsMonitorProperty(ProcFsMonitor::ResultRelativenessProperty, toString(ProcFsMonitor::ResultRelativeness::ABSOLUTE));

  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::string& path, const std::string& filename) -> bool {
      FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
          document.IsObject() &&
          document.HasMember("CPU") &&
          document["CPU"].HasMember("cpu") &&
          document.HasMember("Process") &&
          document.HasMember("Memory") &&
          document.HasMember("Network") &&
          document.HasMember("Disk");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}

TEST_CASE("ProcFsMonitorRelativeTest Json", "[procfsmonitorrelativetestjson]") {
  ProcFsMonitorTester tester;
  tester.setProcFsMonitorProperty(ProcFsMonitor::ResultRelativenessProperty, toString(ProcFsMonitor::ResultRelativeness::RELATIVE));
  tester.setProcFsMonitorProperty(ProcFsMonitor::OutputFormatProperty, toString(ProcFsMonitor::OutputFormat::JSON));
  tester.setProcFsMonitorProperty(ProcFsMonitor::DecimalPlaces, "3");
  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::string& path, const std::string& filename) -> bool {
      FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
          document.IsObject() &&
          document.HasMember("CPU") &&
          document["CPU"].HasMember("cpu") &&
          document.HasMember("Process") &&
          document.HasMember("Memory") &&
          document.HasMember("Network") &&
          document.HasMember("Disk");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}

TEST_CASE("ProcFsMonitorRelativeTest OpenTelemetry", "[procfsmonitorrelativetestopentelemetry]") {
  ProcFsMonitorTester tester;
  tester.setProcFsMonitorProperty(ProcFsMonitor::ResultRelativenessProperty, toString(ProcFsMonitor::ResultRelativeness::RELATIVE));
  tester.setProcFsMonitorProperty(ProcFsMonitor::OutputFormatProperty, toString(ProcFsMonitor::OutputFormat::OPENTELEMETRY));
  tester.setProcFsMonitorProperty(ProcFsMonitor::DecimalPlaces, "3");
  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::string& path, const std::string& filename) -> bool {
      FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
          document.IsObject() &&
          document.HasMember("Body") &&
          document["Body"].HasMember("CPU") &&
          document["Body"]["CPU"].HasMember("cpu") &&
          document["Body"].HasMember("Process") &&
          document["Body"].HasMember("Memory") &&
          document["Body"].HasMember("Network") &&
          document["Body"].HasMember("Disk");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}

