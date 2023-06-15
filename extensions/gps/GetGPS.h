/**
 * @file GetGPS.h
 * GetGPS class declaration
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

#include <array>
#include <string>
#include <memory>
#include <utility>

#include "../FlowFileRecord.h"
#include "../core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"

namespace org::apache::nifi::minifi::processors {

class GetGPS : public core::Processor {
 public:
  explicit GetGPS(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
    gpsdHost_ = "localhost";
    gpsdPort_ = "2947";
    gpsdWaitTime_ = 50000000;
  }
  ~GetGPS() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Obtains GPS coordinates from the GPSDHost and port.";

  EXTENSIONAPI static constexpr auto GPSDHost = core::PropertyDefinitionBuilder<>::createProperty("GPSD Host")
      .withDescription("The host running the GPSD daemon")
      .withDefaultValue("localhost")
      .build();
  EXTENSIONAPI static constexpr auto GPSDPort = core::PropertyDefinitionBuilder<>::createProperty("GPSD Port")
      .withDescription("The GPSD daemon port")
      .withPropertyType(core::StandardPropertyTypes::PORT_TYPE)
      .withDefaultValue("2947")
      .build();
  EXTENSIONAPI static constexpr auto GPSDWaitTime = core::PropertyDefinitionBuilder<>::createProperty("GPSD Wait Time")
      .withDescription("Timeout value for waiting for data from the GPSD instance")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("50000000")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 3>{
      GPSDHost,
      GPSDPort,
      GPSDWaitTime
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  std::string gpsdHost_;
  std::string gpsdPort_;
  int64_t gpsdWaitTime_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetGPS>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
