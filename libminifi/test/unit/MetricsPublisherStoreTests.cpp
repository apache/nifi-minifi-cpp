// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/repository/VolatileContentRepository.h"
#include "properties/Configuration.h"
#include "core/Resource.h"
#include "core/state/MetricsPublisher.h"
#include "core/state/MetricsPublisherStore.h"

namespace org::apache::nifi::minifi::test {

class FirstDummyMetricsPublisher : public minifi::state::MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  static constexpr const char* Description = "FirstDummyMetricsPublisher";

  void clearMetricNodes() override {}
  void loadMetricNodes(core::ProcessGroup*) override {}
};

class SecondDummyMetricsPublisher : public minifi::state::MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  static constexpr const char* Description = "SecondDummyMetricsPublisher";

  void clearMetricNodes() override {}
  void loadMetricNodes(core::ProcessGroup*) override {}
};

REGISTER_RESOURCE(FirstDummyMetricsPublisher, DescriptionOnly);
REGISTER_RESOURCE(SecondDummyMetricsPublisher, DescriptionOnly);

TEST_CASE("Test single metrics publisher store", "[MetricsPublisherStore]") {
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(Configuration::nifi_metrics_publisher_class, "FirstDummyMetricsPublisher");
  minifi::state::MetricsPublisherStore metrics_publisher_store(configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  metrics_publisher_store.initialize(nullptr, nullptr);
  auto publisher = metrics_publisher_store.getMetricsPublisher("FirstDummyMetricsPublisher");
  REQUIRE(publisher.lock());
}

TEST_CASE("Test multiple metrics publisher stores", "[MetricsPublisherStore]") {
  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(Configuration::nifi_metrics_publisher_class, "FirstDummyMetricsPublisher,SecondDummyMetricsPublisher");
  minifi::state::MetricsPublisherStore metrics_publisher_store(configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  metrics_publisher_store.initialize(nullptr, nullptr);
  auto first_publisher = metrics_publisher_store.getMetricsPublisher("FirstDummyMetricsPublisher");
  REQUIRE(first_publisher.lock());
  auto second_publisher = metrics_publisher_store.getMetricsPublisher("SecondDummyMetricsPublisher");
  REQUIRE(second_publisher.lock());
}

}  // namespace org::apache::nifi::minifi::test
