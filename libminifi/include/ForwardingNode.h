// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"

namespace org::apache::nifi::minifi {

class ForwardingNode : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  MINIFIAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  MINIFIAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  MINIFIAPI static constexpr auto Relationships = std::array{Success};
  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;
  MINIFIAPI static constexpr bool IsSingleThreaded = false;

  void initialize() override;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  ~ForwardingNode() override = default;
};

}  // namespace org::apache::nifi::minifi
