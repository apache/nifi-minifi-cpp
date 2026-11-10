// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
    auto publisher_classes = minifi::utils::string::splitAndTrimRemovingEmpty(*metrics_publisher_class_str, ",");
    std::unordered_set<std::string> unique_publisher_classes{publisher_classes.begin(), publisher_classes.end()};
    publishers.reserve(unique_publisher_classes.size());
    for (const auto& publisher_class : unique_publisher_classes) {
      publishers.push_back(createMetricsPublisher(publisher_class, configuration, response_node_loader));
    }
    return publishers;
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::state
