// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/CProcessor.h"

namespace org::apache::nifi::minifi::utils {

std::vector<minifi::state::PublishedMetric> CProcessor::getCustomMetrics() const {
  std::lock_guard guard(metrics_mutex_);
  return reported_metrics_;
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
