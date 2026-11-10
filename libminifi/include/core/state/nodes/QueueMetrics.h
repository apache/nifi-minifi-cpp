// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "core/state/ConnectionStore.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides Connection queue metrics. Provides critical information to the
 * C2 server.
 *
 */
class QueueMetrics : public ResponseNodeImpl {
 public:
  QueueMetrics(const std::string &name, const utils::Identifier &uuid)
     : ResponseNodeImpl(name, uuid) {
  }

  QueueMetrics(const std::string &name) // NOLINT
      : ResponseNodeImpl(name) {
  }

  QueueMetrics()
     : ResponseNodeImpl("QueueMetrics") {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines queue metric information";

  std::string getName() const override {
    return "QueueMetrics";
  }

  void updateConnection(minifi::Connection* connection) {
    connection_store_.updateConnection(connection);
  }

  std::vector<SerializedResponseNode> serialize() override;

  std::vector<PublishedMetric> calculateMetrics() override {
    return connection_store_.calculateConnectionMetrics("QueueMetrics");
  }

 private:
  ConnectionStore connection_store_;
};

}  // namespace org::apache::nifi::minifi::state::response
