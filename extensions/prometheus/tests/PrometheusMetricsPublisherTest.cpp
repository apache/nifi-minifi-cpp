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
#include <memory>
#include <vector>
#include <algorithm>

#include "Catch.h"
#include "PrometheusMetricsPublisher.h"
#include "properties/Configure.h"
#include "MetricsExposer.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "core/RepositoryFactory.h"
#include "range/v3/algorithm/find.hpp"

namespace org::apache::nifi::minifi::extensions::prometheus::test {

class DummyMetricsExposer : public MetricsExposer {
 public:
  void registerMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) override {
    metrics_.push_back(metric);
  }

  void removeMetric(const std::shared_ptr<PublishedMetricGaugeCollection>&) override {
  }

  [[nodiscard]] std::vector<std::shared_ptr<PublishedMetricGaugeCollection>> getMetrics() const {
    return metrics_;
  }

 private:
  std::vector<std::shared_ptr<PublishedMetricGaugeCollection>> metrics_;
};

class PrometheusPublisherTestFixture {
 public:
  explicit PrometheusPublisherTestFixture(bool user_dummy_exposer)
    : configuration_(std::make_shared<Configure>()),
      provenance_repo_(core::createRepository("provenancerepository", true)),
      flow_file_repo_(core::createRepository("flowfilerepository", true)),
      response_node_loader_(configuration_, provenance_repo_, flow_file_repo_, nullptr) {
    std::unique_ptr<DummyMetricsExposer> dummy_exposer;
    if (user_dummy_exposer) {
      dummy_exposer = std::make_unique<DummyMetricsExposer>();
      exposer_ = dummy_exposer.get();
    }
    publisher_ = std::make_unique<PrometheusMetricsPublisher>("publisher", utils::Identifier(), std::move(dummy_exposer));
  }

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  state::response::ResponseNodeLoader response_node_loader_;
  std::unique_ptr<PrometheusMetricsPublisher> publisher_;
  DummyMetricsExposer* exposer_ = nullptr;
};

class PrometheusPublisherTestFixtureWithRealExposer : public PrometheusPublisherTestFixture {
 public:
  PrometheusPublisherTestFixtureWithRealExposer() : PrometheusPublisherTestFixture(false) {}
};

class PrometheusPublisherTestFixtureWithDummyExposer : public PrometheusPublisherTestFixture {
 public:
  PrometheusPublisherTestFixtureWithDummyExposer() : PrometheusPublisherTestFixture(true) {}
};

TEST_CASE_METHOD(PrometheusPublisherTestFixtureWithRealExposer, "Test prometheus empty port", "[prometheusPublisherTest]") {
  REQUIRE_THROWS_WITH(publisher_->initialize(configuration_, response_node_loader_, nullptr), "General Operation: Port not configured for Prometheus metrics publisher!");
}

TEST_CASE_METHOD(PrometheusPublisherTestFixtureWithRealExposer, "Test prometheus invalid port", "[prometheusPublisherTest]") {
  configuration_->set(Configure::nifi_metrics_publisher_port, "invalid");
  REQUIRE_THROWS_AS(publisher_->initialize(configuration_, response_node_loader_, nullptr), std::exception);
}

TEST_CASE_METHOD(PrometheusPublisherTestFixtureWithDummyExposer, "Test adding metrics to exposer", "[prometheusPublisherTest]") {
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "QueueMetrics,RepositoryMetrics,DeviceInfoNode,FlowInformation,AgentInformation,InvalidMetrics,GetFileMetrics,GetTCPMetrics");
  publisher_->initialize(configuration_, response_node_loader_, nullptr);
  auto stored_metrics = exposer_->getMetrics();
  std::vector<std::string> valid_metrics_without_flow = {"QueueMetrics", "RepositoryMetrics", "DeviceInfoNode", "FlowInformation", "AgentInformation"};
  REQUIRE(stored_metrics.size() == valid_metrics_without_flow.size());
  for (const auto& stored_metric : stored_metrics) {
    auto collection = stored_metric->Collect();
    for (const auto& metric_family : collection) {
      for (const auto& prometheus_metric : metric_family.metric) {
        for (const auto& label : prometheus_metric.label) {
          if (label.name == "metric_class") {
            REQUIRE(ranges::find(valid_metrics_without_flow, label.value) != ranges::end(valid_metrics_without_flow));
          }
        }
      }
    }
  }
}

}  // namespace org::apache::nifi::minifi::extensions::prometheus::test
