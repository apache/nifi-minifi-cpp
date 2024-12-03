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
#pragma once

#include <string>
#include <vector>

#include "Connection.h"
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
