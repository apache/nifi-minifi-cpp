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

namespace {

std::vector<minifi::state::PublishedMetric> toPublishedMetrics(OWNED MinifiPublishedMetrics* metrics) {
  return *std::unique_ptr<std::vector<minifi::state::PublishedMetric>>(reinterpret_cast<std::vector<minifi::state::PublishedMetric>*>(metrics));
}

}  // namespace

std::vector<minifi::state::PublishedMetric> CProcessor::getCustomMetrics() const {
  if (class_description_.callbacks.calculateMetrics == nullptr) {
    return {};
  }
  return toPublishedMetrics(class_description_.callbacks.calculateMetrics(impl_));
}

std::string CProcessorMetricsWrapper::CProcessorInfoProvider::getProcessorType() const {
  return source_processor_.getProcessorType();
}
std::string CProcessorMetricsWrapper::CProcessorInfoProvider::getName() const {
  return source_processor_.getName();
}
minifi::utils::SmallString<36> CProcessorMetricsWrapper::CProcessorInfoProvider::getUUIDStr() const {
  return source_processor_.getUUID().to_string();
}

std::vector<minifi::state::response::SerializedResponseNode> CProcessorMetricsWrapper::serialize() {
  auto nodes = ProcessorMetricsImpl::serialize();
  gsl_Assert(!nodes.empty());
  for (auto& custom_node : source_processor_.getCustomMetrics()) {
    size_t transformed_name_length = 0;
    bool should_uppercase_next_letter = true;
    for (size_t i = 0; i < custom_node.name.size(); i++) {
      if (should_uppercase_next_letter) {
        custom_node.name[transformed_name_length++] = static_cast<char>(std::toupper(static_cast<unsigned char>(custom_node.name[i])));
        should_uppercase_next_letter = false;
      } else if (custom_node.name[i] == '_') {
        should_uppercase_next_letter = true;
      } else {
        custom_node.name[transformed_name_length++] = custom_node.name[i];
      }
    }
    custom_node.name.resize(transformed_name_length);
    nodes[0].children.push_back(minifi::state::response::SerializedResponseNode{.name = std::move(custom_node.name), .value = static_cast<uint64_t>(custom_node.value)});
  }
  return nodes;
}

std::vector<minifi::state::PublishedMetric> CProcessorMetricsWrapper::calculateMetrics() {
  auto nodes = ProcessorMetricsImpl::calculateMetrics();
  for (auto& custom_node : source_processor_.getCustomMetrics()) {
    custom_node.labels = getCommonLabels();
    nodes.push_back(std::move(custom_node));
  }
  return nodes;
}

}  // namespace org::apache::nifi::minifi::utils
