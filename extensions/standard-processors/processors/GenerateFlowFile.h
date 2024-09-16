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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class GenerateFlowFile : public core::ProcessorImpl {
 public:
  explicit GenerateFlowFile(const std::string_view name, const utils::Identifier& uuid = {}) // NOLINT
      : ProcessorImpl(name, uuid) {
  }
  ~GenerateFlowFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "This processor creates FlowFiles with random data or custom content. "
      "GenerateFlowFile is useful for load testing, configuration, and simulation.";

  EXTENSIONAPI static constexpr auto FileSize = core::PropertyDefinitionBuilder<>::createProperty("File Size")
      .withDescription("The size of the file that will be used")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1 kB")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("The number of FlowFiles to be transferred in each invocation")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto DataFormat = core::PropertyDefinitionBuilder<2>::createProperty("Data Format")
      .withDescription("Specifies whether the data should be Text or Binary")
      .isRequired(false)
      .withAllowedValues({"Text", "Binary"})
      .withDefaultValue("Binary")
      .build();
  EXTENSIONAPI static constexpr auto UniqueFlowFiles = core::PropertyDefinitionBuilder<>::createProperty("Unique FlowFiles")
      .withDescription("If true, each FlowFile that is generated will be unique. "
          "If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput "
          "(but see the description of Custom Text for special non-random use cases)")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto CustomText = core::PropertyDefinitionBuilder<>::createProperty("Custom Text")
      .withDescription("If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. "
          "Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      FileSize,
      BatchSize,
      DataFormat,
      UniqueFlowFiles,
      CustomText
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const char *DATA_FORMAT_TEXT;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  void refreshNonUniqueData(core::ProcessContext& context);

 private:
  enum class Mode {
    UniqueByte,
    UniqueText,
    NotUniqueByte,
    NotUniqueText,
    CustomText,
    Empty
  };

  Mode mode_;

  std::vector<char> non_unique_data_;

  uint64_t batch_size_{1};
  uint64_t file_size_{1024};

  static Mode getMode(bool is_unique, bool is_text, bool has_custom_text, uint64_t file_size);
  static bool isUnique(Mode mode) { return mode == Mode::UniqueText || mode == Mode::UniqueByte; }
  static bool isText(Mode mode) { return mode == Mode::UniqueText || mode == Mode::CustomText || mode == Mode::NotUniqueText; }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GenerateFlowFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
