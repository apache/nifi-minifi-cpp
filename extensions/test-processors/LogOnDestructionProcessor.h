// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/ProcessorImpl.h"

namespace org::apache::nifi::minifi::processors {

class LogOnDestructionProcessor : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  ~LogOnDestructionProcessor() override {
    logger_->log_info("LogOnDestructionProcessor is being destructed");
  }

  EXTENSIONAPI static constexpr const char* Description = "This processor logs a message on destruction. Only for testing purposes.";
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

}  // namespace org::apache::nifi::minifi::processors
