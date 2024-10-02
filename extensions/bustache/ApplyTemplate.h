/**
 * @file ApplyTemplate.h
 * ApplyTemplate class declaration
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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::processors {

/**
 * Applies a mustache template using incoming attributes as template parameters.
 */
class ApplyTemplate final : public core::ProcessorImpl {
 public:
  explicit ApplyTemplate(const std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {}

  EXTENSIONAPI static constexpr const char* Description = "Applies the mustache template specified by the \"Template\" property and writes the output to the flow file content. "
    "FlowFile attributes are used as template parameters.";

  EXTENSIONAPI static constexpr core::PropertyDefinition Template = core::PropertyDefinitionBuilder<>::createProperty("Template")
      .withDescription("Path to the input mustache template file")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({Template});

  EXTENSIONAPI static constexpr core::RelationshipDefinition Success{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ApplyTemplate>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
