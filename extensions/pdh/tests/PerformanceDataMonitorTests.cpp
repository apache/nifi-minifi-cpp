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
#include <set>
#include <fstream>
#include <functional>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "unit/TestUtils.h"
#include "PerformanceDataMonitor.h"
#include "rapidjson/filereadstream.h"

using org::apache::nifi::minifi::processors::PutFile;
using org::apache::nifi::minifi::processors::PerformanceDataMonitor;
using org::apache::nifi::minifi::processors::PerformanceDataCounter;

class PerformanceDataMonitorTester {
 public:
  PerformanceDataMonitorTester() {
    LogTestController::getInstance().setTrace<TestPlan>();
    dir_ = test_controller_.createTempDirectory();
    plan_ = test_controller_.createPlan();
    performance_monitor_ = plan_->addProcessor("PerformanceDataMonitor", "pdhsys");
    putfile_ = plan_->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);
    plan_->setProperty(putfile_, PutFile::Directory, dir_.string());
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

  void setPerformanceMonitorProperty(const core::PropertyReference& property, const std::string& value) {
    plan_->setProperty(performance_monitor_, property, value);
  }

  TestController test_controller_;
  std::filesystem::path dir_;
  std::shared_ptr<TestPlan> plan_;
  core::Processor* performance_monitor_;
  core::Processor* putfile_;
};


TEST_CASE("PerformanceDataMonitorEmptyPropertiesTest", "[performancedatamonitoremptypropertiestest]") {
  PerformanceDataMonitorTester tester;
  tester.test_controller_.runSession(tester.plan_);
  REQUIRE(tester.test_controller_.getLog().getInstance().contains("No valid counters for PerformanceDataMonitor", std::chrono::seconds(0)));

  auto created_flow_files = utils::file::FileUtils::list_dir_all(tester.dir_, tester.plan_->getLogger(), false);
  REQUIRE(created_flow_files.size() == 0);
}

TEST_CASE("PerformanceDataMonitorPartiallyInvalidGroupPropertyTest", "[performancedatamonitorpartiallyinvalidgrouppropertytest]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "Disk,CPU,Asd");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\Invalid\\Counter,\\System\\Processes");
  const auto assertions = [&tester]() {
    REQUIRE(tester.test_controller_.getLog().getInstance().contains("Asd is not a valid predefined group", std::chrono::seconds(0)));
    REQUIRE(tester.test_controller_.getLog().getInstance().contains("Error adding \\Invalid\\Counter to query", std::chrono::seconds(0)));

    bool ff_contains_all_data = false;

    const auto lambda = [&ff_contains_all_data](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      FILE* fp = fopen((path / filename).string().c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[500];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
        document.IsObject() &&
        document.HasMember("LogicalDisk") &&
        document["LogicalDisk"].HasMember("_Total") &&
        document["LogicalDisk"]["_Total"].HasMember("Free Megabytes") &&
        document["System"].HasMember("Processes");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };
  REQUIRE(tester.runWithRetries(assertions));
}

TEST_CASE("PerformanceDataMonitorCustomPDHCountersTest", "[performancedatamonitorcustompdhcounterstest]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\System\\Processes,\\Process(*)\\% Processor Time");

  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      FILE* fp = fopen((path / filename).string().c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
        document.IsObject() &&
        document.HasMember("System") &&
        document["System"].HasMember("Processes") &&
        document.HasMember("Process") &&
        document["Process"].HasMember("PerformanceDataMonitorTests") &&
        document["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}

TEST_CASE("PerformanceDataMonitorCustomPDHCountersTestOpenTelemetry", "[performancedatamonitorcustompdhcounterstestopentelemetry]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "Disk");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\System\\Processes,\\Process(*)\\ID Process,\\Process(*)\\Private Bytes");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "OpenTelemetry");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputCompactness, "Compact");

  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      FILE* fp = fopen((path / filename).string().c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
        document.IsObject() &&
        document.HasMember("Name") &&
        document.HasMember("Timestamp") &&
        document.HasMember("Body") &&
        document["Body"].HasMember("System") &&
        document["Body"]["System"].HasMember("Processes") &&
        document["Body"].HasMember("Process") &&
        document["Body"]["Process"].HasMember("PerformanceDataMonitorTests") &&
        document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}

TEST_CASE("PerformanceDataMonitorAllPredefinedGroups", "[performancedatamonitorallpredefinedgroups]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "CPU,Disk,Network,Memory,IO,System,Process");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "OpenTelemetry");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputCompactness, "Pretty");

  const auto assertions = [&tester]() {
    bool ff_contains_all_data = false;
    const auto lambda = [&ff_contains_all_data](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      FILE* fp = fopen((path / filename).string().c_str(), "r");
      REQUIRE(fp != nullptr);
      char readBuffer[50000];
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      rapidjson::Document document;
      document.ParseStream(is);
      fclose(fp);
      ff_contains_all_data =
        document.IsObject() &&
        document.HasMember("Name") &&
        document.HasMember("Timestamp") &&
        document.HasMember("Body") &&
        document["Body"].HasMember("PhysicalDisk") &&
        document["Body"]["PhysicalDisk"].HasMember("_Total") &&
        document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Read Time") &&
        document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Time") &&
        document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Write Time") &&
        document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Idle Time") &&

        document["Body"].HasMember("LogicalDisk") &&
        document["Body"]["LogicalDisk"].HasMember("_Total") &&
        document["Body"]["LogicalDisk"]["_Total"].HasMember("Free Megabytes") &&
        document["Body"]["LogicalDisk"]["_Total"].HasMember("% Free Space") &&

        document["Body"].HasMember("Processor") &&
        document["Body"]["Processor"].HasMember("_Total") &&

        document["Body"].HasMember("Network Interface") &&

        document["Body"].HasMember("Memory") &&
        document["Body"]["Memory"].HasMember("% Committed Bytes In Use") &&
        document["Body"]["Memory"].HasMember("Available MBytes") &&
        document["Body"]["Memory"].HasMember("Page Faults/sec") &&
        document["Body"]["Memory"].HasMember("Pages/sec") &&

        document["Body"].HasMember("System") &&
        document["Body"]["System"].HasMember("% Registry Quota In Use") &&
        document["Body"]["System"].HasMember("Context Switches/sec") &&
        document["Body"]["System"].HasMember("File Control Bytes/sec") &&
        document["Body"]["System"].HasMember("File Control Operations/sec") &&

        document["Body"].HasMember("Process") &&
        document["Body"]["Process"].HasMember("PerformanceDataMonitorTests") &&
        document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time") &&
        document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Elapsed Time") &&
        document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process") &&
        document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Private Bytes");
      return !ff_contains_all_data;
    };

    utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
    return ff_contains_all_data;
  };

  REQUIRE(tester.runWithRetries(assertions));
}
