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

#include "TestBase.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "PerformanceDataMonitor.h"
#include "rapidjson/filereadstream.h"

using PutFile = org::apache::nifi::minifi::processors::PutFile;
using PerformanceDataMonitor = org::apache::nifi::minifi::processors::PerformanceDataMonitor;
using PerformanceDataCounter = org::apache::nifi::minifi::processors::PerformanceDataCounter;

class PerformanceDataMonitorTester {
 public:
  PerformanceDataMonitorTester() {
    LogTestController::getInstance().setTrace<TestPlan>();
    dir_ = test_controller_.createTempDirectory("/tmp/gt.XXXXXX");
    plan_ = test_controller_.createPlan();
    performance_monitor_ = plan_->addProcessor("PerformanceDataMonitor", "pdhsys");
    putfile_ = plan_->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);
    plan_->setProperty(putfile_, PutFile::Directory.getName(), dir_);
  }

  void runProcessors() {
    plan_->runNextProcessor();      // PerformanceMonitor
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    plan_->runCurrentProcessor();   // PerformanceMonitor
    plan_->runNextProcessor();      // PutFile
    plan_->runCurrentProcessor();   // PutFile
  }

  void setPerformanceMonitorProperty(core::Property property, const std::string& value) {
    plan_->setProperty(performance_monitor_, property.getName(), value);
  }

  TestController test_controller_;
  std::string dir_;
  std::shared_ptr<TestPlan> plan_;
  std::shared_ptr<core::Processor> performance_monitor_;
  std::shared_ptr<core::Processor> putfile_;
};


TEST_CASE("PerformanceDataMonitorEmptyPropertiesTest", "[performancedatamonitoremptypropertiestest]") {
  PerformanceDataMonitorTester tester;
  tester.runProcessors();

  REQUIRE(tester.test_controller_.getLog().getInstance().contains("No valid counters for PerformanceDataMonitor", std::chrono::seconds(0)));

  uint32_t number_of_flowfiles = 0;
  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 0);
}

TEST_CASE("PerformanceDataMonitorPartiallyInvalidGroupPropertyTest", "[performancedatamonitorpartiallyinvalidgrouppropertytest]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "Disk,CPU,Asd");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\Invalid\\Counter,\\System\\Processes");
  tester.runProcessors();

  REQUIRE(tester.test_controller_.getLog().getInstance().contains("Asd is not a valid predefined group", std::chrono::seconds(0)));
  REQUIRE(tester.test_controller_.getLog().getInstance().contains("Error adding \\Invalid\\Counter to query", std::chrono::seconds(0)));

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[500];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("LogicalDisk"));
    REQUIRE(d["LogicalDisk"].HasMember("_Total"));
    REQUIRE(d["LogicalDisk"]["_Total"].HasMember("Free Megabytes"));
    REQUIRE(d["System"].HasMember("Processes"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorCustomPDHCountersTest", "[performancedatamonitorcustompdhcounterstest]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\System\\Processes,\\Process(*)\\% Processor Time");
  tester.runProcessors();

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[50000];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("System"));
    REQUIRE(d["System"].HasMember("Processes"));
    REQUIRE(d.HasMember("Process"));
    REQUIRE(d["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(d["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorCustomPDHCountersTestOpenTelemetry", "[performancedatamonitorcustompdhcounterstestopentelemetry]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\System\\Processes,\\Process(*)\\ID Process");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "OpenTelemetry");
  tester.runProcessors();

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[50000];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("Name"));
    REQUIRE(d.HasMember("Timestamp"));
    REQUIRE(d.HasMember("Body"));
    REQUIRE(d["Body"].HasMember("System"));
    REQUIRE(d["Body"]["System"].HasMember("Processes"));
    REQUIRE(d["Body"].HasMember("Process"));
    REQUIRE(d["Body"]["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(d["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorAllPredefinedGroups", "[performancedatamonitorallpredefinedgroups]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "CPU,Disk,Network,Memory,IO,System,Process");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "OpenTelemetry");
  tester.runProcessors();

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[50000];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("Name"));
    REQUIRE(d.HasMember("Timestamp"));
    REQUIRE(d.HasMember("Body"));
    REQUIRE(d["Body"].HasMember("PhysicalDisk"));
    REQUIRE(d["Body"]["PhysicalDisk"].HasMember("_Total"));
    REQUIRE(d["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Read Time"));
    REQUIRE(d["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Time"));
    REQUIRE(d["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Write Time"));
    REQUIRE(d["Body"]["PhysicalDisk"]["_Total"].HasMember("% Idle Time"));

    REQUIRE(d["Body"].HasMember("LogicalDisk"));
    REQUIRE(d["Body"]["LogicalDisk"].HasMember("_Total"));
    REQUIRE(d["Body"]["LogicalDisk"]["_Total"].HasMember("Free Megabytes"));
    REQUIRE(d["Body"]["LogicalDisk"]["_Total"].HasMember("% Free Space"));

    REQUIRE(d["Body"].HasMember("Processor"));
    REQUIRE(d["Body"]["Processor"].HasMember("_Total"));

    REQUIRE(d["Body"].HasMember("Network Interface"));

    REQUIRE(d["Body"].HasMember("Memory"));
    REQUIRE(d["Body"]["Memory"].HasMember("% Committed Bytes In Use"));
    REQUIRE(d["Body"]["Memory"].HasMember("Available MBytes"));
    REQUIRE(d["Body"]["Memory"].HasMember("Page Faults/sec"));
    REQUIRE(d["Body"]["Memory"].HasMember("Pages/sec"));

    REQUIRE(d["Body"].HasMember("System"));
    REQUIRE(d["Body"]["System"].HasMember("% Registry Quota In Use"));
    REQUIRE(d["Body"]["System"].HasMember("Context Switches/sec"));
    REQUIRE(d["Body"]["System"].HasMember("File Control Bytes/sec"));
    REQUIRE(d["Body"]["System"].HasMember("File Control Operations/sec"));

    REQUIRE(d["Body"].HasMember("Process"));
    REQUIRE(d["Body"]["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(d["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time"));
    REQUIRE(d["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Elapsed Time"));
    REQUIRE(d["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process"));
    REQUIRE(d["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Private Bytes"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

class MockCounter : public org::apache::nifi::minifi::processors::PerformanceDataCounter {
 public:
  explicit MockCounter(bool& is_active) : is_active_(is_active) {
    is_active_ = true;
  }
  ~MockCounter() { is_active_ = false; }
  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const override {}

  bool& is_active_;
};

class TestablePerformanceDataMonitor : public PerformanceDataMonitor {
 public:
  explicit TestablePerformanceDataMonitor(const std::string& name, utils::Identifier uuid = utils::Identifier())
    : PerformanceDataMonitor(name, uuid) {
  }

  void addCounter(PerformanceDataCounter* counter) {
    resource_consumption_counters_.push_back(counter);
  }
};

TEST_CASE("PerformanceMonitorDeletesItsCountersWhenItGetsDeleted", "[performancemonitordeletesitscounterswhenitgetsdeleted]") {
  bool mock_alive = false;
  {
    TestablePerformanceDataMonitor tester("TestablePerformanceDataMonitor");
    tester.addCounter(new MockCounter(mock_alive));
    REQUIRE(mock_alive);
  }
  REQUIRE_FALSE(mock_alive);
}
