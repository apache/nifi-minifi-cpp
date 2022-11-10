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
#include <utility>

#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi::processors {

class LogOnDestructionProcessor : public core::Processor {
 public:
  explicit LogOnDestructionProcessor(std::string name, const utils::Identifier& uuid = utils::Identifier())
    : Processor(std::move(name), uuid) {
  }

  ~LogOnDestructionProcessor() override {
    logger_->log_info("LogOnDestructionProcessor is being destructed");
  }

  EXTENSIONAPI static constexpr const char* Description = "This processor logs a message on destruction. Only for testing purposes.";
  static auto properties() { return std::array<core::Property, 0>{}; }
  static auto relationships() { return std::array<core::Relationship, 0>{}; }
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LogOnDestructionProcessor>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
