// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "minifi-cpp/Connection.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::state {

class ConnectionStore {
 public:
  const std::unordered_map<utils::Identifier, minifi::Connection*>& getConnections() {
    return connections_;
  }

  void updateConnection(minifi::Connection* connection) {
    if (nullptr != connection) {
      connections_[connection->getUUID()] = connection;
    }
  }

  std::vector<PublishedMetric> calculateConnectionMetrics(const std::string& metric_class) {
    std::vector<PublishedMetric> metrics;

    for (const auto& [_, connection] : connections_) {
      metrics.push_back({"queue_data_size", static_cast<double>(connection->getQueueDataSize()),
        {{"connection_uuid", connection->getUUIDStr()}, {"connection_name", connection->getName()}, {"metric_class", metric_class}}});
      metrics.push_back({"queue_data_size_max", static_cast<double>(connection->getBackpressureThresholdDataSize()),
        {{"connection_uuid", connection->getUUIDStr()}, {"connection_name", connection->getName()}, {"metric_class", metric_class}}});
      metrics.push_back({"queue_size", static_cast<double>(connection->getQueueSize()),
        {{"connection_uuid", connection->getUUIDStr()}, {"connection_name", connection->getName()}, {"metric_class", metric_class}}});
      metrics.push_back({"queue_size_max", static_cast<double>(connection->getBackpressureThresholdCount()),
        {{"connection_uuid", connection->getUUIDStr()}, {"connection_name", connection->getName()}, {"metric_class", metric_class}}});
    }

    return metrics;
  }

 protected:
  std::unordered_map<utils::Identifier, minifi::Connection*> connections_;
};

}  // namespace org::apache::nifi::minifi::state
