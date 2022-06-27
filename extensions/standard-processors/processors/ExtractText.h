/**
 * @file ExtractText.h
 * ExtractText class declaration
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
#include <vector>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "FlowFileRecord.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class ExtractText : public core::Processor {
 public:
  explicit ExtractText(const std::string& name,  const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Extracts the content of a FlowFile and places it into an attribute.";

  EXTENSIONAPI static core::Property Attribute;
  EXTENSIONAPI static core::Property SizeLimit;
  EXTENSIONAPI static core::Property RegexMode;
  EXTENSIONAPI static core::Property IncludeCaptureGroupZero;
  EXTENSIONAPI static core::Property InsensitiveMatch;
  EXTENSIONAPI static core::Property MaxCaptureGroupLen;
  EXTENSIONAPI static core::Property EnableRepeatingCaptureGroup;
  static auto properties() {
    return std::array{
      Attribute,
      SizeLimit,
      RegexMode,
      IncludeCaptureGroupZero,
      InsensitiveMatch,
      MaxCaptureGroupLen,
      EnableRepeatingCaptureGroup
    };
  }

  EXTENSIONAPI static core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  //! Default maximum bytes to read into an attribute
  EXTENSIONAPI static constexpr int DEFAULT_SIZE_LIMIT = 2 * 1024 * 1024;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

  class ReadCallback {
   public:
    ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ct, std::shared_ptr<core::logging::Logger> lgr);
    int64_t operator()(const std::shared_ptr<io::BaseStream>& stream) const;

   private:
    std::shared_ptr<core::FlowFile> flowFile_;
    core::ProcessContext *ctx_;
    std::shared_ptr<core::logging::Logger> logger_;
  };

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExtractText>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
