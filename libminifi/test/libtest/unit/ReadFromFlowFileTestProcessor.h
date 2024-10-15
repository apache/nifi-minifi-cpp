/**
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

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/RelationshipDefinition.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#pragma once

namespace org::apache::nifi::minifi::processors {

class ReadFromFlowFileTestProcessor : public core::ProcessorImpl {
 public:
  static constexpr const char* ON_SCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onSchedule executed";
  static constexpr const char* ON_TRIGGER_LOG_STR = "ReadFromFlowFileTestProcessor::onTrigger executed";
  static constexpr const char* ON_UNSCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onUnSchedule executed";

  explicit ReadFromFlowFileTestProcessor(std::string_view name, const utils::Identifier& uuid = utils::Identifier())
      : ProcessorImpl(name, uuid) {
  }

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
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ReadFromFlowFileTestProcessor>::getLogger(uuid_);
  std::vector<FlowFileData> flow_files_read_;
};

}  // namespace org::apache::nifi::minifi::processors
