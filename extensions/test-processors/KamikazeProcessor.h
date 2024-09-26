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

#include <memory>
#include <string>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"

#pragma once

namespace org::apache::nifi::minifi::processors {

class KamikazeProcessor : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static const std::string OnScheduleExceptionStr;
  EXTENSIONAPI static const std::string OnTriggerExceptionStr;
  EXTENSIONAPI static const std::string OnScheduleLogStr;
  EXTENSIONAPI static const std::string OnTriggerLogStr;
  EXTENSIONAPI static const std::string OnUnScheduleLogStr;

  explicit KamikazeProcessor(std::string_view name, const utils::Identifier& uuid = utils::Identifier())
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "This processor can throw exceptions in onTrigger and onSchedule calls based on configuration. Only for testing purposes.";

  EXTENSIONAPI static constexpr auto ThrowInOnSchedule = core::PropertyDefinitionBuilder<>::createProperty("Throw in onSchedule")
      .withDescription("Set to throw expcetion during onSchedule call")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto ThrowInOnTrigger = core::PropertyDefinitionBuilder<>::createProperty("Throw in onTrigger")
      .withDescription("Set to throw expcetion during onTrigger call")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ThrowInOnSchedule,
      ThrowInOnTrigger
  });


  EXTENSIONAPI static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onUnSchedule() override;

 private:
  bool _throwInOnTrigger = false;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<KamikazeProcessor>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
