/**
 * @file GenerateFlowFile.h
 * GenerateFlowFile class declaration
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
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class GenerateFlowFile : public core::Processor {
 public:
  GenerateFlowFile(std::string name, const utils::Identifier& uuid = {}) // NOLINT
      : Processor(std::move(name), uuid) {
    batchSize_ = 1;
    uniqueFlowFile_ = true;
    fileSize_ = 1024;
    textData_ = false;
  }
  ~GenerateFlowFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "This processor creates FlowFiles with random data or custom content. "
      "GenerateFlowFile is useful for load testing, configuration, and simulation.";

  EXTENSIONAPI static const core::Property FileSize;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property DataFormat;
  EXTENSIONAPI static const core::Property UniqueFlowFiles;
  EXTENSIONAPI static const core::Property CustomText;
  static auto properties() {
    return std::array{
      FileSize,
      BatchSize,
      DataFormat,
      UniqueFlowFiles,
      CustomText
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const char *DATA_FORMAT_TEXT;

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 protected:
  std::vector<char> data_;

  uint64_t batchSize_;
  bool uniqueFlowFile_;
  uint64_t fileSize_;
  bool textData_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GenerateFlowFile>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
