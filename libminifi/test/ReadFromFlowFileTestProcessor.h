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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#pragma once

namespace org::apache::nifi::minifi::processors {

class ReadFromFlowFileTestProcessor : public core::Processor {
 public:
  static constexpr const char* ON_SCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onSchedule executed";
  static constexpr const char* ON_TRIGGER_LOG_STR = "ReadFromFlowFileTestProcessor::onTrigger executed";
  static constexpr const char* ON_UNSCHEDULE_LOG_STR = "ReadFromFlowFileTestProcessor::onUnSchedule executed";

  explicit ReadFromFlowFileTestProcessor(const std::string& name, const utils::Identifier& uuid = utils::Identifier())
      : Processor(name, uuid) {
  }

  static constexpr char const* const ProcessorName = "ReadFromFlowFileTestProcessor";
  static const core::Relationship Success;

 public:
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  void onUnSchedule() override;

  const std::string& getContent() {
    return content_;
  }

 private:
  std::shared_ptr<logging::Logger> logger_ = logging::LoggerFactory<ReadFromFlowFileTestProcessor>::getLogger();
  std::string content_;
};

REGISTER_RESOURCE(ReadFromFlowFileTestProcessor, "ReadFromFlowFileTestProcessor (only for testing purposes)");

}  // namespace org::apache::nifi::minifi::processors
