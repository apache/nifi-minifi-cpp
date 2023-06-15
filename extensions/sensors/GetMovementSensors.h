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
#include <regex>
#include <string>
#include <utility>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/RelationshipDefinition.h"
#include "utils/Id.h"
#include "SensorBase.h"
#include "RTIMULib.h"
#include "RTMath.h"

namespace org::apache::nifi::minifi::processors {

class GetMovementSensors : public SensorBase {
 public:
  explicit GetMovementSensors(std::string name, const utils::Identifier& uuid = {})
      : SensorBase(std::move(name), uuid) {
  }
  virtual ~GetMovementSensors();

  EXTENSIONAPI static constexpr const char* Description = "Defines a processor that is able to retrieve sensor information from a class of known servo sensors";
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetMovementSensors>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
