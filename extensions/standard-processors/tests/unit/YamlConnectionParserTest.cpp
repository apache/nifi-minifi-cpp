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

#include "core/yaml/YamlConnectionParser.h"

#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "TestBase.h"
#include "utils/TestUtils.h"

namespace {

using org::apache::nifi::minifi::core::yaml::YamlConnectionParser;
using org::apache::nifi::minifi::core::YamlConfiguration;
using RetryFlowFile = org::apache::nifi::minifi::processors::TailFile;

TEST_CASE("Connections components are parsed from yaml.", "[YamlConfiguration]") {
  const std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<YamlConfiguration>::getLogger();
  core::ProcessGroup parent(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
  gsl::not_null<core::ProcessGroup*> parent_ptr{ &parent };

  SECTION("Source relationships are read") {
    const auto connection = std::make_shared<minifi::Connection>(nullptr, nullptr, "name");
    std::string serialized_yaml;
    std::vector<std::string> expectations;
    SECTION("Single relationship name") {
      serialized_yaml = std::string { "source relationship name: success\n" };
      expectations = { "success" };
    }
    SECTION("List of relationship names") {
      serialized_yaml = std::string {
          "source relationship names:\n"
          "- success\n"
          "- failure\n"
          "- something_else\n" };
      expectations = { "success", "failure", "something_else" };
    }
    YAML::Node connection_node = YAML::Load(serialized_yaml);
    YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    yaml_connection_parser.configureConnectionSourceRelationshipsFromYaml(connection);
    const std::set<core::Relationship>& relationships = connection->getRelationships();
    REQUIRE(expectations.size() == relationships.size());
    for (const auto& expected_relationship_name : expectations) {
      const auto relationship_name_matches = [&] (const core::Relationship& relationship) { return expected_relationship_name == relationship.getName(); };
      const std::size_t relationship_count = std::count_if(relationships.cbegin(), relationships.cend(), relationship_name_matches);
      REQUIRE(1 == relationship_count);
    }
  }
  SECTION("Queue size limits are read") {
    YAML::Node connection_node = YAML::Load(std::string {
        "max work queue size: 231\n"
        "max work queue data size: 12 MB\n" });
    YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(231 == yaml_connection_parser.getWorkQueueSizeFromYaml());
    REQUIRE(12582912 == yaml_connection_parser.getWorkQueueDataSizeFromYaml());  // 12 * 1024 * 1024 B
  }
  SECTION("Source and destination names and uuids are read") {
    const utils::Identifier expected_source_id = utils::generateUUID();
    const utils::Identifier expected_destination_id = utils::generateUUID();
    std::string serialized_yaml;
    parent.addProcessor(std::static_pointer_cast<core::Processor>(std::make_shared<processors::TailFile>("TailFile_1", expected_source_id)));
    parent.addProcessor(std::static_pointer_cast<core::Processor>(std::make_shared<processors::TailFile>("TailFile_2", expected_destination_id)));
    SECTION("Directly from configuration") {
      serialized_yaml = std::string {
          "source id: " + expected_source_id.to_string() + "\n"
          "destination id: " + expected_destination_id.to_string() + "\n" };
    }
    SECTION("Using UUID as remote port id") {
      serialized_yaml = std::string {
          "source name: " + expected_source_id.to_string() + "\n"
          "destination name: " + expected_destination_id.to_string() + "\n" };
    }
    SECTION("Via processor name lookup") {
      serialized_yaml = std::string {
          "source name: TailFile_1\n"
          "destination name: TailFile_2\n" };
    }
    YAML::Node connection_node = YAML::Load(serialized_yaml);
    YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(expected_source_id == yaml_connection_parser.getSourceUUIDFromYaml());
    REQUIRE(expected_destination_id == yaml_connection_parser.getDestinationUUIDFromYaml());
  }
  SECTION("Flow file expiration is read") {
    YAML::Node connection_node = YAML::Load(std::string {
        "flowfile expiration: 2 min\n" });
    YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(120000 == yaml_connection_parser.getFlowFileExpirationFromYaml());  // 2 * 60 * 1000 ms
  }
  SECTION("Drop empty value is read") {
    SECTION("When config contains true value") {
      YAML::Node connection_node = YAML::Load(std::string {
          "drop empty: true\n" });
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      REQUIRE(true == yaml_connection_parser.getDropEmptyFromYaml());
    }
    SECTION("When config contains false value") {
      YAML::Node connection_node = YAML::Load(std::string {
          "drop empty: false\n" });
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      REQUIRE(false == yaml_connection_parser.getDropEmptyFromYaml());
    }
  }
  SECTION("Errors are handled properly when configuration lines are missing") {
    const auto connection = std::make_shared<minifi::Connection>(nullptr, nullptr, "name");
    SECTION("With empty configuration") {
      YAML::Node connection_node = YAML::Load(std::string(""));
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      CHECK_THROWS(yaml_connection_parser.configureConnectionSourceRelationshipsFromYaml(connection));
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueSizeFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueDataSizeFromYaml());
      CHECK_THROWS(yaml_connection_parser.getSourceUUIDFromYaml());
      CHECK_THROWS(yaml_connection_parser.getDestinationUUIDFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getFlowFileExpirationFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getDropEmptyFromYaml());
    }
    SECTION("With a configuration that lists keys but has no assigned values") {
      std::string serialized_yaml;
      SECTION("Single relationship name left empty") {
        YAML::Node connection_node = YAML::Load(std::string {
            "source name: \n"
            "destination name: \n" });
        YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        // This seems incorrect, but we do not want to ruin backward compatibility
        CHECK_NOTHROW(yaml_connection_parser.configureConnectionSourceRelationshipsFromYaml(connection));
      }
      SECTION("List of relationship names contains empty item") {
        YAML::Node connection_node = YAML::Load(std::string {
            "source relationship names:\n"
            "- \n" });
        YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_THROWS(yaml_connection_parser.configureConnectionSourceRelationshipsFromYaml(connection));
      }
      SECTION("Source and destination lookup from via id") {
        YAML::Node connection_node = YAML::Load(std::string {
            "source id: \n"
            "destination id: \n" });
        YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_THROWS(yaml_connection_parser.getSourceUUIDFromYaml());
        CHECK_THROWS(yaml_connection_parser.getDestinationUUIDFromYaml());
      }
      SECTION("Source and destination lookup via name") {
        YAML::Node connection_node = YAML::Load(std::string {
            "source name: \n"
            "destination name: \n" });
        YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_THROWS(yaml_connection_parser.getSourceUUIDFromYaml());
        CHECK_THROWS(yaml_connection_parser.getDestinationUUIDFromYaml());
      }
      YAML::Node connection_node = YAML::Load(std::string {
          "max work queue size: \n"
          "max work queue data size: \n"
          "flowfile expiration: \n"
          "drop empty: \n"});
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      CHECK_THROWS(yaml_connection_parser.getWorkQueueSizeFromYaml());
      CHECK_THROWS(yaml_connection_parser.getWorkQueueDataSizeFromYaml());
      CHECK_THROWS(yaml_connection_parser.getFlowFileExpirationFromYaml());
      CHECK_THROWS(yaml_connection_parser.getDropEmptyFromYaml());
    }
    SECTION("With a configuration that has values of incorrect format") {
      YAML::Node connection_node = YAML::Load(std::string {
          "max work queue size: 2 KB\n"
          "max work queue data size: 10 Incorrect\n"
          "flowfile expiration: 12\n"
          "drop empty: sup\n"});
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueSizeFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueDataSizeFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getFlowFileExpirationFromYaml());
      CHECK_NOTHROW(yaml_connection_parser.getDropEmptyFromYaml());
    }
    SECTION("Known incorrect formats that behave strangely") {
      YAML::Node connection_node = YAML::Load(std::string {
          "max work queue data size: 2 Baby Pandas (img, 20 MB) that are cared for by a group of 30 giraffes\n"
          "flowfile expiration: 0\n"
          "drop empty: NULL\n"});
      YamlConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      REQUIRE(2 == yaml_connection_parser.getWorkQueueDataSizeFromYaml());
      REQUIRE(0 == yaml_connection_parser.getFlowFileExpirationFromYaml());
      CHECK_THROWS(yaml_connection_parser.getDropEmptyFromYaml());
    }
  }
}

}  // namespace
