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

#include "SingleProcessorTestController.h"
#include "Catch.h"
#include "processors/ProcFsMonitor.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

TEST_CASE("ProcFsMonitorTests", "[procfsmonitortests]") {
  std::shared_ptr<ProcFsMonitor> proc_fs_monitor = std::make_shared<ProcFsMonitor>("ProcFsMonitor");
  org::apache::nifi::minifi::test::SingleProcessorTestController test_controller_{proc_fs_monitor};

  SECTION("Absolute JSON") {
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::ResultRelativenessProperty, "Absolute");
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::OutputFormatProperty, "JSON");
    const auto& result = test_controller_.trigger();

    REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
    const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

    rapidjson::Document document;
    auto content = test_controller_.plan->getContent(result_flow_file);
    document.Parse(content.c_str());
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("CPU"));
    CHECK(document["CPU"].HasMember("cpu"));
    CHECK(document.HasMember("Disk"));
    CHECK(document.HasMember("Network"));
    CHECK(document.HasMember("Process"));
    CHECK(document.HasMember("Memory"));
  }

  SECTION("Absolute OpenTelemetry")  {
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::ResultRelativenessProperty, "Absolute");
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::OutputFormatProperty, "OpenTelemetry");
    const auto& result = test_controller_.trigger();

    REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
    const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

    rapidjson::Document document;
    auto content = test_controller_.plan->getContent(result_flow_file);
    document.Parse(content.c_str());
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("Body"));
    REQUIRE(document["Body"].HasMember("CPU"));
    CHECK(document["Body"]["CPU"].HasMember("cpu"));
    CHECK(document["Body"].HasMember("Disk"));
    CHECK(document["Body"].HasMember("Network"));
    CHECK(document["Body"].HasMember("Process"));
    CHECK(document["Body"].HasMember("Memory"));
  }

  SECTION("Relative JSON") {
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::ResultRelativenessProperty, "Relative");
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::OutputFormatProperty, "JSON");
    {
      const auto& result = test_controller_.trigger();

      REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
      const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

      rapidjson::Document document;
      auto content = test_controller_.plan->getContent(result_flow_file);
      document.Parse(content.c_str());
      REQUIRE(document.IsObject());
      // First trigger has not enough information for relative output
      CHECK_FALSE(document.HasMember("CPU"));
      CHECK_FALSE(document.HasMember("Disk"));
      CHECK_FALSE(document.HasMember("Network"));
      CHECK_FALSE(document.HasMember("Process"));
      CHECK(document.HasMember("Memory"));
    }
    std::this_thread::sleep_for(100ms);
    {
      const auto& result = test_controller_.trigger();

      REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
      const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

      rapidjson::Document document;
      auto content = test_controller_.plan->getContent(result_flow_file);
      document.Parse(content.c_str());
      REQUIRE(document.IsObject());
      CHECK(document.HasMember("CPU"));
      CHECK(document.HasMember("Disk"));
      CHECK(document.HasMember("Network"));
      CHECK(document.HasMember("Process"));
      CHECK(document.HasMember("Memory"));
    }
  }

  SECTION("Relative OpenTelemetry") {
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::ResultRelativenessProperty, "Relative");
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::OutputFormatProperty, "OpenTelemetry");
    {
      const auto& result = test_controller_.trigger();

      REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
      const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

      rapidjson::Document document;
      auto content = test_controller_.plan->getContent(result_flow_file);
      document.Parse(content.c_str());
      REQUIRE(document.IsObject());
      REQUIRE(document.HasMember("Body"));
      // First trigger has not enough information for relative output
      CHECK_FALSE(document["Body"].HasMember("CPU"));
      CHECK_FALSE(document["Body"].HasMember("Disk"));
      CHECK_FALSE(document["Body"].HasMember("Network"));
      CHECK_FALSE(document["Body"].HasMember("Process"));
      CHECK(document["Body"].HasMember("Memory"));
    }
    std::this_thread::sleep_for(100ms);
    {
      const auto& result = test_controller_.trigger();

      REQUIRE(result.at(ProcFsMonitor::Success).size() == 1);
      const auto& result_flow_file = result.at(ProcFsMonitor::Success)[0];

      rapidjson::Document document;
      auto content = test_controller_.plan->getContent(result_flow_file);
      document.Parse(content.c_str());
      REQUIRE(document.IsObject());
      REQUIRE(document.HasMember("Body"));
      CHECK(document["Body"].HasMember("CPU"));
      CHECK(document["Body"].HasMember("Disk"));
      CHECK(document["Body"].HasMember("Network"));
      CHECK(document["Body"].HasMember("Process"));
      CHECK(document["Body"].HasMember("Memory"));
    }
  }

  SECTION("Relative without wait") {
    test_controller_.plan->setProperty(proc_fs_monitor, ProcFsMonitor::ResultRelativenessProperty, "Relative");
    const auto& result1 = test_controller_.trigger();
    REQUIRE(result1.at(ProcFsMonitor::Success).size() == 1);
    const auto& result2 = test_controller_.trigger();
    REQUIRE(result2.at(ProcFsMonitor::Success).size() == 1);
  }
}
}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
