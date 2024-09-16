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
#include <string_view>
#include <utility>
#include <vector>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class ExtractText : public core::ProcessorImpl {
 public:
  explicit ExtractText(std::string_view name,  const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  // Default maximum bytes to read into an attribute
  static constexpr std::string_view DEFAULT_SIZE_LIMIT_STR = "2097152";  // 2 * 1024 * 1024
  static constexpr std::string_view MAX_CAPTURE_GROUP_SIZE_STR = "1024";

  EXTENSIONAPI static constexpr const char* Description = "Extracts the content of a FlowFile and places it into an attribute.";

  EXTENSIONAPI static constexpr auto Attribute = core::PropertyDefinitionBuilder<>::createProperty("Attribute")
      .withDescription("Attribute to set from content")
      .build();
  // despite there being a size value, ExtractText was initially built with a numeric for this property
  EXTENSIONAPI static constexpr auto SizeLimit = core::PropertyDefinitionBuilder<>::createProperty("Size Limit")
      .withDescription("Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue(DEFAULT_SIZE_LIMIT_STR)
      .build();
  EXTENSIONAPI static constexpr auto RegexMode = core::PropertyDefinitionBuilder<>::createProperty("Regex Mode")
      .withDescription("Set this to extract parts of flowfile content using regular experssions in dynamic properties")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto IncludeCaptureGroupZero = core::PropertyDefinitionBuilder<>::createProperty("Include Capture Group 0")
      .withDescription("Indicates that Capture Group 0 should be included as an attribute. "
         "Capture Group 0 represents the entirety of the regular expression match, is typically not used, and could have considerable length.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto InsensitiveMatch = core::PropertyDefinitionBuilder<>::createProperty("Enable Case-insensitive Matching")
      .withDescription("Indicates that two characters match even if they are in a different case. ")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto MaxCaptureGroupLen = core::PropertyDefinitionBuilder<>::createProperty("Maximum Capture Group Length")
      .withDescription("Specifies the maximum number of characters a given capture group value can have. "
        "Any characters beyond the max will be truncated.")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue(MAX_CAPTURE_GROUP_SIZE_STR)
      .build();
  EXTENSIONAPI static constexpr auto EnableRepeatingCaptureGroup = core::PropertyDefinitionBuilder<>::createProperty("Enable repeating capture group")
      .withDescription("f set to true, every string matching the capture groups will be extracted. "
                      "Otherwise, if the Regular Expression matches more than once, only the first match will be extracted.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Attribute,
      SizeLimit,
      RegexMode,
      IncludeCaptureGroupZero,
      InsensitiveMatch,
      MaxCaptureGroupLen,
      EnableRepeatingCaptureGroup
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  class ReadCallback {
   public:
    ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ct, std::shared_ptr<core::logging::Logger> lgr);
    int64_t operator()(const std::shared_ptr<io::InputStream>& stream) const;

   private:
    std::shared_ptr<core::FlowFile> flowFile_;
    core::ProcessContext *ctx_;
    std::shared_ptr<core::logging::Logger> logger_;
  };

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExtractText>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
