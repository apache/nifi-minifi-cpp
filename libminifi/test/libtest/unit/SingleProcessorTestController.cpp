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

#include <set>

#include "FlowFileRecord.h"
#include "range/v3/algorithm/all_of.hpp"

namespace org::apache::nifi::minifi::test {

ProcessorTriggerResult SingleProcessorTestController::trigger() {
  plan->runProcessor(processor_);
  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> result;
  for (const auto& [relationship, connection]: outgoing_connections_) {
    std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;
    std::vector<std::shared_ptr<core::FlowFile>> output_flow_files;
    while (connection->isWorkAvailable()) {
      auto output_flow_file = connection->poll(expired_flow_files);
      assert(expired_flow_files.empty());
      if (!output_flow_file) continue;
      output_flow_files.push_back(std::move(output_flow_file));
    }
    result.insert_or_assign(relationship, std::move(output_flow_files));
  }
  return result;
}

ProcessorTriggerResult SingleProcessorTestController::trigger(InputFlowFileData&& input_flow_file_data) {
  input_->put(createFlowFile(input_flow_file_data.content, std::move(input_flow_file_data.attributes)));
  return trigger();
}

ProcessorTriggerResult SingleProcessorTestController::trigger(const std::string_view input_flow_file_content, std::unordered_map<std::string, std::string> input_flow_file_attributes) {
  return trigger({input_flow_file_content, std::move(input_flow_file_attributes)});
}

ProcessorTriggerResult SingleProcessorTestController::trigger(std::vector<InputFlowFileData>&& input_flow_file_datas) {
  for (auto& input_flow_file_data : std::move(input_flow_file_datas)) {
    input_->put(createFlowFile(input_flow_file_data.content, std::move(input_flow_file_data.attributes)));
  }
  return trigger();
}

bool SingleProcessorTestController::triggerUntil(const std::unordered_map<core::Relationship, size_t>& expected_quantities,
                  ProcessorTriggerResult& result,
                  const std::chrono::milliseconds max_duration,
                  const std::chrono::milliseconds wait_time) {
  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < start_time + max_duration) {
    for (auto& [relationship, flow_files] : trigger()) {
      result[relationship].insert(result[relationship].end(), flow_files.begin(), flow_files.end());
    }
    if (ranges::all_of(expected_quantities, [&result](const auto& kv) {
      const auto& [relationship, expected_quantity] = kv;
      return result[relationship].size() >= expected_quantity;
    })) {
      return true;
    }
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

core::Relationship SingleProcessorTestController::addDynamicRelationship(std::string name) {
  auto relationship = core::Relationship{std::move(name), ""};
  outgoing_connections_.insert_or_assign(relationship, plan->addConnection(processor_, relationship, nullptr));
  return relationship;
}

std::shared_ptr<core::FlowFile> SingleProcessorTestController::createFlowFile(const std::string_view content, std::unordered_map<std::string, std::string> attributes) {
  const auto flow_file = std::make_shared<FlowFileRecordImpl>();
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

}  // namespace org::apache::nifi::minifi::test
