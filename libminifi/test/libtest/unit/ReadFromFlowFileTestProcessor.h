// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "core/Resource.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#pragma once

namespace org::apache::nifi::minifi::processors {

class ReadFromFlowFileTestProcessor : public core::ProcessorImpl {
 public:
  static constexpr const char* ON_SCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onSchedule executed";
  static constexpr const char* ON_TRIGGER_LOG_STR = "ReadFromFlowFileTestProcessor::onTrigger executed";
  static constexpr const char* ON_UNSCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onUnSchedule executed";

  using ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "ReadFromFlowFileTestProcessor (only for testing purposes)";

  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  static constexpr auto Relationships = std::array{Success};

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onUnSchedule() override;

  bool readFlowFileWithContent(const std::string& content) const;
  bool readFlowFileWithAttribute(const std::string& key) const;
  bool readFlowFileWithAttribute(const std::string& key, const std::string& value) const;

  size_t numberOfFlowFilesRead() const {
    return flow_files_read_.size();
  }

  void enableClearOnTrigger() {
    clear_on_trigger_ = true;
  }

  void disableClearOnTrigger() {
    clear_on_trigger_ = false;
  }

  void clear() {
    flow_files_read_.clear();
  }

 private:
  struct FlowFileData {
    FlowFileData(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file);
    std::string content_;
    std::map<std::string, std::string> attributes_;
  };
  bool clear_on_trigger_ = true;
  std::vector<FlowFileData> flow_files_read_;
};

}  // namespace org::apache::nifi::minifi::processors
