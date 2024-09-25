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
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/repository/VolatileContentRepository.h"
#include "properties/Configuration.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::test {

class FirstDummyMetricsPublisher : public minifi::state::MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  static constexpr const char* Description = "FirstDummyMetricsPublisher";

  void clearMetricNodes() override {}
  void loadMetricNodes() override {}
};

class SecondDummyMetricsPublisher : public minifi::state::MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  static constexpr const char* Description = "SecondDummyMetricsPublisher";

  void clearMetricNodes() override {}
  void loadMetricNodes() override {}
};

REGISTER_RESOURCE(FirstDummyMetricsPublisher, DescriptionOnly);
REGISTER_RESOURCE(SecondDummyMetricsPublisher, DescriptionOnly);

TEST_CASE("Test single metrics publisher store", "[MetricsPublisherStore]") {
  auto configuration = std::make_shared<minifi::Configure>();
  configuration->set(Configuration::nifi_metrics_publisher_class, "FirstDummyMetricsPublisher");
  minifi::state::MetricsPublisherStore metrics_publisher_store(configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  metrics_publisher_store.initialize(nullptr, nullptr);
  auto publisher = metrics_publisher_store.getMetricsPublisher("FirstDummyMetricsPublisher");
  REQUIRE(publisher.lock());
}

TEST_CASE("Test multiple metrics publisher stores", "[MetricsPublisherStore]") {
  auto configuration = std::make_shared<minifi::Configure>();
  configuration->set(Configuration::nifi_metrics_publisher_class, "FirstDummyMetricsPublisher,SecondDummyMetricsPublisher");
  minifi::state::MetricsPublisherStore metrics_publisher_store(configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr);
  metrics_publisher_store.initialize(nullptr, nullptr);
  auto first_publisher = metrics_publisher_store.getMetricsPublisher("FirstDummyMetricsPublisher");
  REQUIRE(first_publisher.lock());
  auto second_publisher = metrics_publisher_store.getMetricsPublisher("SecondDummyMetricsPublisher");
  REQUIRE(second_publisher.lock());
}

}  // namespace org::apache::nifi::minifi::test
