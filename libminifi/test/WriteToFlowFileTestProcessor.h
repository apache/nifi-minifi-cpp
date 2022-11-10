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

#include <string>
#include <memory>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#pragma once

namespace org::apache::nifi::minifi::processors {

class WriteToFlowFileTestProcessor : public core::Processor {
 public:
  static constexpr const char* ON_SCHEDULE_LOG_STR = "WriteToFlowFileTestProcessor::onSchedule executed";
  static constexpr const char* ON_TRIGGER_LOG_STR = "WriteToFlowFileTestProcessor::onTrigger executed";
  static constexpr const char* ON_UNSCHEDULE_LOG_STR = "WriteToFlowFileTestProcessor::onUnSchedule executed";

  explicit WriteToFlowFileTestProcessor(std::string name, const utils::Identifier& uuid = utils::Identifier())
      : Processor(std::move(name), uuid) {
  }

  static constexpr const char* Description = "WriteToFlowFileTestProcessor (only for testing purposes)";
  static auto properties() { return std::array<core::Property, 0>{}; }
  static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

 public:
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  void onUnSchedule() override;

  void setContent(std::string content) {
    content_ = std::move(content);
  }

  void clearContent() {
    content_.clear();
  }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<WriteToFlowFileTestProcessor>::getLogger();
  std::string content_;
};

}  // namespace org::apache::nifi::minifi::processors
