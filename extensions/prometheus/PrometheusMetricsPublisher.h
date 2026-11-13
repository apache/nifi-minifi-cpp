// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <mutex>

#include "core/state/MetricsPublisher.h"
#include "PublishedMetricGaugeCollection.h"
#include "core/Core.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Id.h"
#include "PrometheusExposerWrapper.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PrometheusMetricsPublisher : public state::MetricsPublisherImpl {
 public:
  explicit PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid = {}, std::unique_ptr<MetricsExposer> exposer = nullptr);

  EXTENSIONAPI static constexpr const char* Description = "HTTP server that exposes MiNiFi metrics for Prometheus to scrape";

  void initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) override;
  void clearMetricNodes() override;
  void loadMetricNodes(core::ProcessGroup* root) override;

 private:
  PrometheusExposerConfig readExposerConfig() const;
  std::vector<gsl::not_null<std::shared_ptr<state::PublishedMetricProvider>>> getMetricProviders(core::ProcessGroup* root) const;
  void loadAgentIdentifier();

  std::mutex registered_metrics_mutex_;
  std::shared_ptr<PublishedMetricGaugeCollection> gauge_collection_;
  std::unique_ptr<MetricsExposer> exposer_;
  std::string agent_identifier_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PrometheusMetricsPublisher>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
