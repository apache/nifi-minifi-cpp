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
#include "core/state/MetricsPublisherFactory.h"

#include "utils/StringUtils.h"
#include "core/ClassLoader.h"

namespace org::apache::nifi::minifi::state {

gsl::not_null<std::unique_ptr<MetricsPublisher>> createMetricsPublisher(const std::string& name, const std::shared_ptr<Configure>& configuration,
    const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) {
  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(name, name);
  if (!ptr) {
    throw std::runtime_error("Configured metrics publisher class \"" + name + "\" could not be instantiated.");
  }

  auto metrics_publisher = utils::dynamic_unique_cast<MetricsPublisher>(std::move(ptr));
  if (!metrics_publisher) {
    throw std::runtime_error("Configured metrics publisher class \"" + name + "\" is not a MetricsPublisher.");
  }

  metrics_publisher->initialize(configuration, response_node_loader);
  return gsl::make_not_null(std::move(metrics_publisher));
}

std::vector<gsl::not_null<std::unique_ptr<MetricsPublisher>>> createMetricsPublishers(
    const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) {
  if (auto metrics_publisher_class_str = configuration->get(minifi::Configure::nifi_metrics_publisher_class)) {
    std::vector<gsl::not_null<std::unique_ptr<MetricsPublisher>>> publishers;
    auto publisher_classes = minifi::utils::string::split(*metrics_publisher_class_str, ",");
    publishers.reserve(publisher_classes.size());
    for (const auto& publisher_class : publisher_classes) {
      publishers.push_back(createMetricsPublisher(publisher_class, configuration, response_node_loader));
    }
    return publishers;
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::state
