// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "minifi-cpp/core/RepositoryMetricsSource.h"
#include "RepositoryMetricsSourceStore.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides repository metrics. Provides critical information to the
 * C2 server.
 *
 */
class RepositoryMetrics : public ResponseNodeImpl {
 public:
  RepositoryMetrics(std::string_view name, const utils::Identifier &uuid)
      : ResponseNodeImpl(name, uuid),
        repository_metrics_source_store_(getName()) {
  }

  explicit RepositoryMetrics(std::string_view name)
      : ResponseNodeImpl(name),
        repository_metrics_source_store_(getName()) {
  }

  RepositoryMetrics()
      : ResponseNodeImpl("RepositoryMetrics"),
        repository_metrics_source_store_(getName()) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines repository metric information";

  std::string getName() const override {
    return "RepositoryMetrics";
  }

  void addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo);
  std::vector<SerializedResponseNode> serialize() override;
  std::vector<PublishedMetric> calculateMetrics() override;

 protected:
  RepositoryMetricsSourceStore repository_metrics_source_store_;
};

}  // namespace org::apache::nifi::minifi::state::response
