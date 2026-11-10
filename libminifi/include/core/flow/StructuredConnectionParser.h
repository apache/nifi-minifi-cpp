// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
  void addFunnelOrPortRelationshipToConnection(minifi::Connection& connection) const;

  const Node& connectionNode_;
  const std::string& name_;
  gsl::not_null<core::ProcessGroup*> parent_;
  const std::shared_ptr<logging::Logger> logger_;
  const FlowSchema schema_;
};

}  // namespace org::apache::nifi::minifi::core::flow
