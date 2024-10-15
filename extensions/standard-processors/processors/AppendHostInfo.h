/**
 * @file AppendHostInfo.h
 * AppendHostInfo class declaration
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
#include <optional>
#include <regex>
#include <shared_mutex>
#include <string>
#include <utility>

#include "core/Property.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class AppendHostInfo : public core::ProcessorImpl {
 public:
  static constexpr const char* REFRESH_POLICY_ON_TRIGGER = "On every trigger";
  static constexpr const char* REFRESH_POLICY_ON_SCHEDULE = "On schedule";

  explicit AppendHostInfo(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }
  ~AppendHostInfo() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Appends host information such as IP address and hostname as an attribute to incoming flowfiles.";

  EXTENSIONAPI static constexpr auto InterfaceNameFilter = core::PropertyDefinitionBuilder<>::createProperty("Network Interface Filter")
      .withDescription("A regular expression to filter ip addresses based on the name of the network interface")
      .build();
  EXTENSIONAPI static constexpr auto HostAttribute = core::PropertyDefinitionBuilder<>::createProperty("Hostname Attribute")
      .withDescription("Flowfile attribute used to record the agent's hostname")
      .withDefaultValue("source.hostname")
      .build();
  EXTENSIONAPI static constexpr auto IPAttribute = core::PropertyDefinitionBuilder<>::createProperty("IP Attribute")
      .withDescription("Flowfile attribute used to record the agent's IP addresses in a comma separated list")
      .withDefaultValue("source.ipv4")
      .build();
  EXTENSIONAPI static constexpr auto RefreshPolicy = core::PropertyDefinitionBuilder<2>::createProperty("Refresh Policy")
      .withDescription("When to recalculate the host info")
      .withAllowedValues({ REFRESH_POLICY_ON_SCHEDULE, REFRESH_POLICY_ON_TRIGGER })
      .withDefaultValue(REFRESH_POLICY_ON_SCHEDULE)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      InterfaceNameFilter,
      HostAttribute,
      IPAttribute,
      RefreshPolicy
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 protected:
  virtual void refreshHostInfo();

 private:
  std::shared_mutex shared_mutex_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AppendHostInfo>::getLogger(uuid_);
  std::string hostname_attribute_name_;
  std::string ipaddress_attribute_name_;
  std::optional<std::regex> interface_name_filter_;
  bool refresh_on_trigger_ = false;

  std::string hostname_;
  std::optional<std::string> ipaddresses_;
};

}  // namespace org::apache::nifi::minifi::processors
