/**
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

#include "utils/CProcessor.h"

namespace org::apache::nifi::minifi::utils {

std::vector<minifi::state::PublishedMetric> CProcessor::getCustomMetrics() const {
  if (class_description_.callbacks.calculateMetrics == nullptr) {
    return {};
  }
  std::unique_ptr<std::vector<minifi::state::PublishedMetric>> metrics{reinterpret_cast<std::vector<minifi::state::PublishedMetric>*>(class_description_.callbacks.calculateMetrics(impl_))};
  return *metrics;
}

std::vector<minifi::state::response::SerializedResponseNode> CProcessorMetricsWrapper::serialize() {
  std::vector<minifi::state::response::SerializedResponseNode> nodes;
  for (auto& custom_node : source_processor_.getCustomMetrics()) {
    nodes.push_back(minifi::state::response::SerializedResponseNode{
      .name = utils::string::snakeCaseToPascalCase(custom_node.name),
      .value = static_cast<uint64_t>(custom_node.value)
    });
  }
  return nodes;
}

std::vector<minifi::state::PublishedMetric> CProcessorMetricsWrapper::calculateMetrics() {
  return source_processor_.getCustomMetrics();
}

}  // namespace org::apache::nifi::minifi::utils
