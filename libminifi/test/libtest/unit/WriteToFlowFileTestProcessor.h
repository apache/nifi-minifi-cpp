// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>

#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "core/Resource.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class WriteToFlowFileTestProcessor : public core::ProcessorImpl {
 public:
  static constexpr const char* ON_SCHEDULE_LOG_STR = "WriteToFlowFileTestProcessor::onSchedule executed";
  static constexpr const char* ON_TRIGGER_LOG_STR = "WriteToFlowFileTestProcessor::onTrigger executed";
  static constexpr const char* ON_UNSCHEDULE_LOG_STR = "WriteToFlowFileTestProcessor::onUnSchedule executed";

  using ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "WriteToFlowFileTestProcessor (only for testing purposes)";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  static constexpr auto Relationships = std::array{Success};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onUnSchedule() override;

  void setContent(std::string content) {
    content_ = std::move(content);
  }

  void clearContent() {
    content_.clear();
  }

 private:
  std::string content_;
};

}  // namespace org::apache::nifi::minifi::processors
