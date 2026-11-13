// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/nodes/RepositoryMetrics.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

void RepositoryMetrics::addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo) {
  repository_metrics_source_store_.addRepository(repo);
}

std::vector<SerializedResponseNode> RepositoryMetrics::serialize() {
  return repository_metrics_source_store_.serialize();
}

std::vector<PublishedMetric> RepositoryMetrics::calculateMetrics() {
  return repository_metrics_source_store_.calculateMetrics();
}

REGISTER_RESOURCE(RepositoryMetrics, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response

