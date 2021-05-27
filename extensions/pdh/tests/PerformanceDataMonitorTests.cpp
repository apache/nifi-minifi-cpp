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
#include "utils/TestUtils.h"
#include "PerformanceDataMonitor.h"
#include "rapidjson/filereadstream.h"

using org::apache::nifi::minifi::processors::PutFile;
using org::apache::nifi::minifi::processors::PerformanceDataMonitor;
using org::apache::nifi::minifi::processors::PerformanceDataCounter;

class PerformanceDataMonitorTester {
 public:
  PerformanceDataMonitorTester() {
    LogTestController::getInstance().setTrace<TestPlan>();
    dir_ = utils::createTempDir(&test_controller_);
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

  void setPerformanceMonitorProperty(const core::Property& property, const std::string& value) {
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

  auto created_flow_files = utils::file::FileUtils::list_dir_all(tester.dir_, tester.plan_->getLogger(), false);
  REQUIRE(created_flow_files.size() == 0);
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
    rapidjson::Document document;
    document.ParseStream(is);
    fclose(fp);
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("LogicalDisk"));
    REQUIRE(document["LogicalDisk"].HasMember("_Total"));
    REQUIRE(document["LogicalDisk"]["_Total"].HasMember("Free Megabytes"));
    REQUIRE(document["System"].HasMember("Processes"));
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
    rapidjson::Document document;
    document.ParseStream(is);
    fclose(fp);
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("System"));
    REQUIRE(document["System"].HasMember("Processes"));
    REQUIRE(document.HasMember("Process"));
    REQUIRE(document["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(document["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorCustomPDHCountersTestOpenTelemetry", "[performancedatamonitorcustompdhcounterstestopentelemetry]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "Disk");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::CustomPDHCounters, "\\System\\Processes,\\Process(*)\\ID Process,\\Process(*)\\Private Bytes");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "Compact OpenTelemetry");
  tester.runProcessors();

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[50000];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document document;
    document.ParseStream(is);
    fclose(fp);
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("Name"));
    REQUIRE(document.HasMember("Timestamp"));
    REQUIRE(document.HasMember("Body"));
    REQUIRE(document["Body"].HasMember("System"));
    REQUIRE(document["Body"]["System"].HasMember("Processes"));
    REQUIRE(document["Body"].HasMember("Process"));
    REQUIRE(document["Body"]["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorAllPredefinedGroups", "[performancedatamonitorallpredefinedgroups]") {
  PerformanceDataMonitorTester tester;
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::PredefinedGroups, "CPU,Disk,Network,Memory,IO,System,Process");
  tester.setPerformanceMonitorProperty(PerformanceDataMonitor::OutputFormatProperty, "Pretty OpenTelemetry");
  tester.runProcessors();

  uint32_t number_of_flowfiles = 0;

  auto lambda = [&number_of_flowfiles](const std::string& path, const std::string& filename) -> bool {
    ++number_of_flowfiles;
    FILE* fp = fopen((path + utils::file::FileUtils::get_separator() + filename).c_str(), "r");
    REQUIRE(fp != nullptr);
    char readBuffer[50000];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document document;
    document.ParseStream(is);
    fclose(fp);
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("Name"));
    REQUIRE(document.HasMember("Timestamp"));
    REQUIRE(document.HasMember("Body"));
    REQUIRE(document["Body"].HasMember("PhysicalDisk"));
    REQUIRE(document["Body"]["PhysicalDisk"].HasMember("_Total"));
    REQUIRE(document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Read Time"));
    REQUIRE(document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Time"));
    REQUIRE(document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Disk Write Time"));
    REQUIRE(document["Body"]["PhysicalDisk"]["_Total"].HasMember("% Idle Time"));

    REQUIRE(document["Body"].HasMember("LogicalDisk"));
    REQUIRE(document["Body"]["LogicalDisk"].HasMember("_Total"));
    REQUIRE(document["Body"]["LogicalDisk"]["_Total"].HasMember("Free Megabytes"));
    REQUIRE(document["Body"]["LogicalDisk"]["_Total"].HasMember("% Free Space"));

    REQUIRE(document["Body"].HasMember("Processor"));
    REQUIRE(document["Body"]["Processor"].HasMember("_Total"));

    REQUIRE(document["Body"].HasMember("Network Interface"));

    REQUIRE(document["Body"].HasMember("Memory"));
    REQUIRE(document["Body"]["Memory"].HasMember("% Committed Bytes In Use"));
    REQUIRE(document["Body"]["Memory"].HasMember("Available MBytes"));
    REQUIRE(document["Body"]["Memory"].HasMember("Page Faults/sec"));
    REQUIRE(document["Body"]["Memory"].HasMember("Pages/sec"));

    REQUIRE(document["Body"].HasMember("System"));
    REQUIRE(document["Body"]["System"].HasMember("% Registry Quota In Use"));
    REQUIRE(document["Body"]["System"].HasMember("Context Switches/sec"));
    REQUIRE(document["Body"]["System"].HasMember("File Control Bytes/sec"));
    REQUIRE(document["Body"]["System"].HasMember("File Control Operations/sec"));

    REQUIRE(document["Body"].HasMember("Process"));
    REQUIRE(document["Body"]["Process"].HasMember("PerformanceDataMonitorTests"));
    REQUIRE(document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("% Processor Time"));
    REQUIRE(document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Elapsed Time"));
    REQUIRE(document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("ID Process"));
    REQUIRE(document["Body"]["Process"]["PerformanceDataMonitorTests"].HasMember("Private Bytes"));
    return true;
  };

  utils::file::FileUtils::list_dir(tester.dir_, lambda, tester.plan_->getLogger(), false);
  REQUIRE(number_of_flowfiles == 2);
}

TEST_CASE("PerformanceDataMonitorDecimalPlacesPropertyTest", "[performancedatamonitordecimalplacespropertytest]") {
  {
    PerformanceDataMonitorTester tester;
    tester.setPerformanceMonitorProperty(PerformanceDataMonitor::DecimalPlaces, "asd");
    REQUIRE_THROWS_WITH(tester.runProcessors(), "General Operation: Invalid conversion to int64_t for asd");
  }
  {
    PerformanceDataMonitorTester tester;
    tester.setPerformanceMonitorProperty(PerformanceDataMonitor::DecimalPlaces, "1234586123");
    REQUIRE_THROWS_WITH(tester.runProcessors(), "Process Schedule Operation: PerformanceDataMonitor Decimal Places is out of range");
  }
}
