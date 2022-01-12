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
#pragma once
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "TestBase.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi::test {
class SingleInputTestController : public TestController {
 public:
  explicit SingleInputTestController(const std::shared_ptr<core::Processor>& processor)
      : processor_{plan->addProcessor(processor, processor->getName())}
  {}

  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>>
  trigger(const std::string_view input_flow_file_content, std::unordered_map<std::string, std::string> input_flow_file_attributes = {}) {
    const auto new_flow_file = createFlowFile(input_flow_file_content, std::move(input_flow_file_attributes));
    input_->put(new_flow_file);
    plan->runProcessor(processor_);
    std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> result;
    for (const auto& [relationship, connection]: outgoing_connections_) {
      std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;
      std::vector<std::shared_ptr<core::FlowFile>> output_flow_files;
      while (connection->isWorkAvailable()) {
        auto output_flow_file = connection->poll(expired_flow_files);
        CHECK(expired_flow_files.empty());
        if (!output_flow_file) continue;
        output_flow_files.push_back(std::move(output_flow_file));
      }
      result.insert_or_assign(relationship, std::move(output_flow_files));
    }
    return result;
  }

  core::Relationship addDynamicRelationship(std::string name) {
    auto relationship = core::Relationship{std::move(name), ""};
    outgoing_connections_.insert_or_assign(relationship, plan->addConnection(processor_, relationship, nullptr));
    return relationship;
  }

 private:
  std::shared_ptr<core::FlowFile> createFlowFile(const std::string_view content, std::unordered_map<std::string, std::string> attributes) {
    const auto flow_file = std::make_shared<FlowFileRecord>();
    for (auto& attr : std::move(attributes)) {
      flow_file->setAttribute(attr.first, std::move(attr.second));
    }
    auto content_session = plan->getContentRepo()->createSession();
    auto claim = content_session->create();
    auto stream = content_session->write(claim);
    stream->write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
    flow_file->setResourceClaim(claim);
    flow_file->setSize(stream->size());
    flow_file->setOffset(0);

    stream->close();
    content_session->commit();
    return flow_file;
  }

 public:
  std::shared_ptr<TestPlan> plan = createPlan();

 protected:
  core::Processor& getProcessor() const { return *processor_; }

 private:
  std::shared_ptr<core::Processor> processor_;
  std::unordered_map<core::Relationship, std::shared_ptr<Connection>> outgoing_connections_{[this] {
    std::unordered_map<core::Relationship, std::shared_ptr<Connection>> result;
    for (const auto& relationship: processor_->getSupportedRelationships()) {
      result.insert_or_assign(relationship, plan->addConnection(processor_, relationship, nullptr));
    }
    return result;
  }()};
  std::shared_ptr<Connection> input_ = plan->addConnection(nullptr, core::Relationship{"success", "success"}, processor_);
};

}  // namespace org::apache::nifi::minifi::test
