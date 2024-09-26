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

#include "core/flow/StructuredConnectionParser.h"

#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "core/yaml/YamlNode.h"

using namespace std::literals::chrono_literals;

namespace {

using org::apache::nifi::minifi::core::flow::StructuredConnectionParser;
using org::apache::nifi::minifi::core::YamlConfiguration;
using RetryFlowFile = org::apache::nifi::minifi::processors::TailFile;
using org::apache::nifi::minifi::core::YamlNode;
namespace flow = org::apache::nifi::minifi::core::flow;

TEST_CASE("Connections components are parsed from yaml", "[YamlConfiguration]") {
  const std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<YamlConfiguration>::getLogger();
  core::ProcessGroup parent(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
  gsl::not_null<core::ProcessGroup*> parent_ptr{ &parent };

  SECTION("Source relationships are read") {
    const auto connection = std::make_shared<minifi::ConnectionImpl>(nullptr, nullptr, "name");
    std::string serialized_yaml;
    std::set<org::apache::nifi::minifi::core::Relationship> expectations;
    SECTION("Single relationship name") {
      serialized_yaml = std::string { "source relationship name: success\n" };
      expectations = { { "success", "" } };
    }
    SECTION("List of relationship names") {
      serialized_yaml = std::string {
          "source relationship names:\n"
          "- success\n"
          "- failure\n"
          "- something_else\n" };
      expectations = { { "success", "" }, { "failure", "" }, { "something_else", "" } };
    }
    YAML::Node yaml_node = YAML::Load(serialized_yaml);
    flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
    StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    yaml_connection_parser.configureConnectionSourceRelationships(*connection);
    const std::set<core::Relationship>& relationships = connection->getRelationships();
    REQUIRE(expectations == relationships);
  }
  SECTION("Queue size limits are read") {
    YAML::Node yaml_node = YAML::Load(std::string {
        "max work queue size: 231\n"
        "max work queue data size: 12 MB\n" });
    flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
    StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(231 == yaml_connection_parser.getWorkQueueSize());
    REQUIRE(12_MiB == yaml_connection_parser.getWorkQueueDataSize());
  }
  SECTION("Queue swap threshold is read") {
    YAML::Node yaml_node = YAML::Load(std::string {
        "swap threshold: 231\n" });
    flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
    StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(231 == yaml_connection_parser.getSwapThreshold());
  }
  SECTION("Source and destination names and uuids are read") {
    const utils::Identifier expected_source_id = minifi::test::utils::generateUUID();
    const utils::Identifier expected_destination_id = minifi::test::utils::generateUUID();
    std::string serialized_yaml;
    parent.addProcessor(std::make_unique<minifi::processors::TailFile>("TailFile_1", expected_source_id));
    parent.addProcessor(std::make_unique<minifi::processors::TailFile>("TailFile_2", expected_destination_id));
    SECTION("Directly from configuration") {
      serialized_yaml = std::string {
          "source id: " + expected_source_id.to_string() + "\n"
          "destination id: " + expected_destination_id.to_string() + "\n" };
    }
    SECTION("Using UUID as remote processing group id") {
      serialized_yaml = std::string {
          "source name: " + expected_source_id.to_string() + "\n"
          "destination name: " + expected_destination_id.to_string() + "\n" };
    }
    SECTION("Via processor name lookup") {
      serialized_yaml = std::string {
          "source name: TailFile_1\n"
          "destination name: TailFile_2\n" };
    }
    YAML::Node yaml_node = YAML::Load(serialized_yaml);
    flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
    StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(expected_source_id == yaml_connection_parser.getSourceUUID());
    REQUIRE(expected_destination_id == yaml_connection_parser.getDestinationUUID());
  }
  SECTION("Flow file expiration is read") {
    YAML::Node yaml_node = YAML::Load(std::string {
        "flowfile expiration: 2 min\n" });
    flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
    StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
    REQUIRE(2min == yaml_connection_parser.getFlowFileExpiration());
  }
  SECTION("Drop empty value is read") {
    SECTION("When config contains true value") {
      YAML::Node yaml_node = YAML::Load(std::string {
          "drop empty: true\n" });
      flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
      StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      REQUIRE(true == yaml_connection_parser.getDropEmpty());
    }
    SECTION("When config contains false value") {
      YAML::Node yaml_node = YAML::Load(std::string {
          "drop empty: false\n" });
      flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
      StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      REQUIRE(false == yaml_connection_parser.getDropEmpty());
    }
  }
  SECTION("Errors are handled properly when configuration lines are missing") {
    const auto connection = std::make_shared<minifi::ConnectionImpl>(nullptr, nullptr, "name");
    SECTION("With empty configuration") {
      YAML::Node yaml_node = YAML::Load(std::string(""));
      flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};

      CHECK_THROWS(StructuredConnectionParser(connection_node, "test_node", parent_ptr, logger));
    }
    SECTION("With a configuration that lists keys but has no assigned values") {
      std::string serialized_yaml;
      SECTION("Single relationship name left empty") {
        YAML::Node yaml_node = YAML::Load(std::string {
            "source name: \n"
            "destination name: \n" });
        flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
        StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        // This seems incorrect, but we do not want to ruin backward compatibility
        CHECK_NOTHROW(yaml_connection_parser.configureConnectionSourceRelationships(*connection));
      }
      SECTION("List of relationship names contains empty item") {
        YAML::Node yaml_node = YAML::Load(std::string {
            "source relationship names:\n"
            "- \n" });
        flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
        StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_NOTHROW(yaml_connection_parser.configureConnectionSourceRelationships(*connection));
      }
      SECTION("Source and destination lookup from via id") {
        YAML::Node yaml_node = YAML::Load(std::string {
            "source id: \n"
            "destination id: \n" });
        flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
        StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_THROWS(yaml_connection_parser.getSourceUUID());
        CHECK_THROWS(yaml_connection_parser.getDestinationUUID());
      }
      SECTION("Source and destination lookup via name") {
        YAML::Node yaml_node = YAML::Load(std::string {
            "source name: \n"
            "destination name: \n" });
        flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
        StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK_THROWS(yaml_connection_parser.getSourceUUID());
        CHECK_THROWS(yaml_connection_parser.getDestinationUUID());
      }
      SECTION("Queue limits and configuration") {
        YAML::Node yaml_node = YAML::Load(std::string {
            "max work queue size: \n"
            "max work queue data size: \n"
            "swap threshold: \n"
            "flowfile expiration: \n"
            "drop empty: \n"});
        flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
        StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
        CHECK(minifi::Connection::DEFAULT_BACKPRESSURE_THRESHOLD_COUNT == yaml_connection_parser.getWorkQueueSize());
        CHECK(minifi::Connection::DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE == yaml_connection_parser.getWorkQueueDataSize());
        CHECK(0 == yaml_connection_parser.getSwapThreshold());
        CHECK(0s == yaml_connection_parser.getFlowFileExpiration());
        CHECK_FALSE(yaml_connection_parser.getDropEmpty());
      }
    }
    SECTION("With a configuration that has values of incorrect format") {
      YAML::Node yaml_node = YAML::Load(std::string {
          "max work queue size: 2 KB\n"
          "max work queue data size: 10 Incorrect\n"
          "flowfile expiration: 12\n"
          "drop empty: sup\n"});
      flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
      StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      // This seems incorrect, but we do not want to ruin backward compatibility
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueSize());
      CHECK_NOTHROW(yaml_connection_parser.getWorkQueueDataSize());
      CHECK_NOTHROW(yaml_connection_parser.getFlowFileExpiration());
      CHECK_NOTHROW(yaml_connection_parser.getDropEmpty());
    }
    SECTION("Known incorrect formats that behave strangely") {
      YAML::Node yaml_node = YAML::Load(std::string {
          "max work queue data size: 2 Baby Pandas (img, 20 MB) that are cared for by a group of 30 giraffes\n"
          "flowfile expiration: 0\n"
          "drop empty: NULL\n"});
      flow::Node connection_node{std::make_shared<YamlNode>(yaml_node)};
      StructuredConnectionParser yaml_connection_parser(connection_node, "test_node", parent_ptr, logger);
      CHECK(2 == yaml_connection_parser.getWorkQueueDataSize());
      CHECK(0s == yaml_connection_parser.getFlowFileExpiration());
      CHECK_FALSE(yaml_connection_parser.getDropEmpty());
    }
  }
}

}  // namespace
