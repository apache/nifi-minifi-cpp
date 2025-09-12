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
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "TestBase.h"
#include "core/Processor.h"
#include "core/ProcessorImpl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {
struct InputFlowFileData {
  std::string_view content;
  std::unordered_map<std::string, std::string> attributes = {};
};

using ProcessorTriggerResult = std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>>;

class SingleProcessorTestController : public TestController {
 public:
  explicit SingleProcessorTestController(std::unique_ptr<core::Processor> processor) {
    auto name = processor->getName();
    processor_ = plan->addProcessor(std::move(processor), name, {});
    input_ = plan->addConnection(nullptr, core::Relationship{"success", "success"}, processor_);
    outgoing_connections_ = [this] {
      std::unordered_map<core::Relationship, Connection*> result;
      for (const auto& relationship: processor_->getSupportedRelationships()) {
        result.insert_or_assign(relationship, plan->addConnection(processor_, relationship, nullptr));
      }
      return result;
    }();
  }

  ProcessorTriggerResult trigger();
  ProcessorTriggerResult trigger(InputFlowFileData input_flow_file_data);
  ProcessorTriggerResult trigger(const std::string_view input_flow_file_content, std::unordered_map<std::string, std::string> input_flow_file_attributes = {});
  ProcessorTriggerResult trigger(std::vector<InputFlowFileData>&& input_flow_file_datas);
  bool triggerUntil(const std::unordered_map<core::Relationship, size_t>& expected_quantities,
                    ProcessorTriggerResult& result,
                    const std::chrono::milliseconds max_duration,
                    const std::chrono::milliseconds wait_time = 50ms);

  core::Relationship addDynamicRelationship(std::string name);

  template<typename T>
  TypedProcessorWrapper<T> getProcessor() const { return processor_; }

  core::Processor* getProcessor() const { return processor_; }

  std::shared_ptr<TestPlan> plan = createPlan();

 private:
  std::shared_ptr<core::FlowFile> createFlowFile(const std::string_view content, std::unordered_map<std::string, std::string> attributes) const;

  core::Processor* processor_ = nullptr;
  std::unordered_map<core::Relationship, Connection*> outgoing_connections_;
  Connection* input_ = nullptr;
};

}  // namespace org::apache::nifi::minifi::test
