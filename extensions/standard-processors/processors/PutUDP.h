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
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::core::logging { class Logger; }

namespace org::apache::nifi::minifi::processors {
class PutUDP final : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet "
      "which is then transmitted to the configured UDP server. "
      "The processor doesn't guarantee a successful transfer, even if the flow file is routed to the success relationship.";

  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
    .withDescription("The ip address or hostname of the destination.")
    .withDefaultValue("localhost")
    .isRequired(true)
    .supportsExpressionLanguage(true)
    .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
    .withDescription("The port on the destination. Can be a service name like ssh or http, as defined in /etc/services.")
    .isRequired(true)
    .supportsExpressionLanguage(true)
    .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({Hostname, Port});

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are sent to the destination are sent out this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles that encountered IO errors are sent out this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutUDP(std::string_view name, const utils::Identifier& uuid = {});
  PutUDP(const PutUDP&) = delete;
  PutUDP& operator=(const PutUDP&) = delete;
  ~PutUDP() final;

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) final;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) final;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};
}  // namespace org::apache::nifi::minifi::processors
