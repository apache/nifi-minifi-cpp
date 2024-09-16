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
#include <unordered_map>
#include "unit/Catch.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "unit/ReadFromFlowFileTestProcessor.h"
#include "unit/WriteToFlowFileTestProcessor.h"
#include "core/repository/VolatileContentRepository.h"
#include "utils/Id.h"
#include "unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::test {

class ResponseNodeLoaderTestFixture {
 public:
  ResponseNodeLoaderTestFixture()
    : root_(std::make_unique<minifi::core::ProcessGroup>(minifi::core::ProcessGroupType::ROOT_PROCESS_GROUP, "root")),
      configuration_(std::make_shared<minifi::ConfigureImpl>()),
      prov_repo_(std::make_shared<TestRepository>()),
      ff_repository_(std::make_shared<TestRepository>()),
      content_repo_(std::make_shared<minifi::core::repository::VolatileContentRepository>()),
      response_node_loader_(configuration_, {prov_repo_, ff_repository_, content_repo_}, nullptr) {
    ff_repository_->initialize(configuration_);
    content_repo_->initialize(configuration_);
    auto uuid1 = addProcessor<minifi::processors::WriteToFlowFileTestProcessor>("WriteToFlowFileTestProcessor1");
    auto uuid2 = addProcessor<minifi::processors::WriteToFlowFileTestProcessor>("WriteToFlowFileTestProcessor1");
    addConnection("Connection", "success", uuid1, uuid2);
    auto uuid3 = addProcessor<minifi::processors::ReadFromFlowFileTestProcessor>("ReadFromFlowFileTestProcessor");
    addConnection("Connection", "success", uuid2, uuid3);
    response_node_loader_.setNewConfigRoot(root_.get());
  }

 protected:
  template<typename T, typename = typename std::enable_if_t<std::is_base_of_v<minifi::core::Processor, T>>>
  minifi::utils::Identifier addProcessor(const std::string& name) {
    auto processor = std::make_unique<T>(name);
    auto uuid = processor->getUUID();
    root_->addProcessor(std::move(processor));
    return uuid;
  }

  void addConnection(const std::string& connection_name, const std::string& relationship_name, const minifi::utils::Identifier& src_uuid, const minifi::utils::Identifier& dst_uuid) {
    auto connection = std::make_unique<minifi::ConnectionImpl>(ff_repository_, content_repo_, connection_name);
    connection->addRelationship({relationship_name, "d"});
    connection->setDestinationUUID(src_uuid);
    connection->setSourceUUID(dst_uuid);
    root_->addConnection(std::move(connection));
  }

  std::unique_ptr<minifi::core::ProcessGroup> root_;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<TestRepository> prov_repo_;
  std::shared_ptr<TestRepository> ff_repository_;
  std::shared_ptr<minifi::core::ContentRepository> content_repo_;
  minifi::state::response::ResponseNodeLoaderImpl response_node_loader_;
};

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Load non-existent response node", "[responseNodeLoaderTest]") {
  auto nodes = response_node_loader_.loadResponseNodes("NonExistentNode");
  REQUIRE(nodes.empty());
}

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Load processor metrics node not part of the flow config", "[responseNodeLoaderTest]") {
  auto nodes = response_node_loader_.loadResponseNodes("TailFileMetrics");
  REQUIRE(nodes.empty());
}

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Load system metrics node", "[responseNodeLoaderTest]") {
  auto nodes = response_node_loader_.loadResponseNodes("QueueMetrics");
  REQUIRE(nodes.size() == 1);
  REQUIRE(nodes[0]->getName() == "QueueMetrics");
}

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Load processor metrics node part of the flow config", "[responseNodeLoaderTest]") {
  auto nodes = response_node_loader_.loadResponseNodes("ReadFromFlowFileTestProcessorMetrics");
  REQUIRE(nodes.size() == 1);
  REQUIRE(nodes[0]->getName() == "ReadFromFlowFileTestProcessorMetrics");
}

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Load multiple processor metrics nodes of the same type in a single flow", "[responseNodeLoaderTest]") {
  auto nodes = response_node_loader_.loadResponseNodes("WriteToFlowFileTestProcessorMetrics");
  REQUIRE(nodes.size() == 2);
  REQUIRE(nodes[0]->getName() == "WriteToFlowFileTestProcessorMetrics");
  REQUIRE(nodes[1]->getName() == "WriteToFlowFileTestProcessorMetrics");
}

TEST_CASE_METHOD(ResponseNodeLoaderTestFixture, "Use regex to filter processor metrics", "[responseNodeLoaderTest]") {
  SECTION("Load all processor metrics with regex") {
    auto nodes = response_node_loader_.loadResponseNodes("processorMetrics/.*");
    std::unordered_map<std::string, uint32_t> metric_counts;
    REQUIRE(nodes.size() == 3);
    for (const auto& node : nodes) {
      metric_counts[node->getName()]++;
    }
    REQUIRE(metric_counts["WriteToFlowFileTestProcessorMetrics"] == 2);
    REQUIRE(metric_counts["ReadFromFlowFileTestProcessorMetrics"] == 1);
  }

  SECTION("Filter for a single processor") {
    auto nodes = response_node_loader_.loadResponseNodes("processorMetrics/Read.*");
    REQUIRE(nodes.size() == 1);
    REQUIRE(nodes[0]->getName() == "ReadFromFlowFileTestProcessorMetrics");
  }

  SECTION("Full match") {
    auto nodes = response_node_loader_.loadResponseNodes("processorMetrics/ReadFromFlowFileTestProcessorMetrics");
    REQUIRE(nodes.size() == 1);
    REQUIRE(nodes[0]->getName() == "ReadFromFlowFileTestProcessorMetrics");
  }

  SECTION("No partial match is allowed") {
    auto nodes = response_node_loader_.loadResponseNodes("processorMetrics/Read");
    REQUIRE(nodes.empty());
  }
}

}  // namespace org::apache::nifi::minifi::test
