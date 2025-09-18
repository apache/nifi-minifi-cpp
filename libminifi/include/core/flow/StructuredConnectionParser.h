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

#include <memory>
#include <string>
#include <string_view>

#include "core/ProcessGroup.h"
#include "core/logging/LoggerFactory.h"

#include "core/flow/Node.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/flow/FlowSchema.h"

namespace org::apache::nifi::minifi::core::flow {

class StructuredConnectionParser {
 public:
  explicit StructuredConnectionParser(const Node& connectionNode, const std::string& name, gsl::not_null<core::ProcessGroup*> parent,
                                      const std::shared_ptr<logging::Logger>& logger, std::optional<FlowSchema> schema = std::nullopt) :
      connectionNode_(connectionNode),
      name_(name),
      parent_(parent),
      logger_(logger),
      schema_(schema.value_or(FlowSchema::getDefault())) {
    if (!connectionNode.isMap()) {
      throw std::logic_error("Connection node is not a map");
    }
  }

  void configureConnectionSourceRelationships(minifi::Connection& connection) const;
  [[nodiscard]] uint64_t getWorkQueueSize() const;
  [[nodiscard]] uint64_t getWorkQueueDataSize() const;
  [[nodiscard]] uint64_t getSwapThreshold() const;
  [[nodiscard]] utils::Identifier getSourceUUID() const;
  [[nodiscard]] utils::Identifier getDestinationUUID() const;
  [[nodiscard]] std::chrono::milliseconds getFlowFileExpiration() const;
  [[nodiscard]] bool getDropEmpty() const;

 private:
  void addNewRelationshipToConnection(std::string_view relationship_name, minifi::Connection& connection) const;
  void addFunnelRelationshipToConnection(minifi::Connection& connection) const;

  const Node& connectionNode_;
  const std::string& name_;
  gsl::not_null<core::ProcessGroup*> parent_;
  const std::shared_ptr<logging::Logger> logger_;
  const FlowSchema schema_;
};

}  // namespace org::apache::nifi::minifi::core::flow
