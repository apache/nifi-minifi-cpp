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

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opc.h"
#include "opcbase.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/Property.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/StateManager.h"

namespace org::apache::nifi::minifi::processors {

enum class LazyModeOptions {
  On,
  NewValue,
  Off
};

}  // namespace org::apache::nifi::minifi::processors

namespace magic_enum::customize {

using LazyModeOptions = org::apache::nifi::minifi::processors::LazyModeOptions;

template <>
constexpr customize_t enum_name<LazyModeOptions>(LazyModeOptions value) noexcept {
  switch (value) {
    case LazyModeOptions::On:
      return "On";
    case LazyModeOptions::NewValue:
      return "New Value";
    case LazyModeOptions::Off:
      return "Off";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class FetchOPCProcessor final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Fetches OPC-UA node";

  EXTENSIONAPI static constexpr auto NodeIDType = core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Node ID type")
      .withDescription("Specifies the type of the provided node ID")
      .isRequired(true)
      .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
      .build();
  EXTENSIONAPI static constexpr auto NodeID = core::PropertyDefinitionBuilder<>::createProperty("Node ID")
      .withDescription("Specifies the ID of the root node to traverse. In case of a Path Node ID Type, the path should be provided in the format of 'path/to/node'.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex = core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
      .withDescription("The index of the namespace.")
      .withValidator(core::StandardPropertyValidators::INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxDepth = core::PropertyDefinitionBuilder<>::createProperty("Max depth")
      .withDescription("Specifiec the max depth of browsing. 0 means unlimited.")
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Lazy = core::PropertyDefinitionBuilder<magic_enum::enum_count<LazyModeOptions>()>::createProperty("Lazy mode")
      .withDescription("Only creates flowfiles from nodes with new timestamp from the server. If set to 'New Value', it will only create flowfiles "
                       "if the value of the node data has changed since the last fetch, the timestamp is ignored.")
      .isRequired(true)
      .withAllowedValues(magic_enum::enum_names<LazyModeOptions>())
      .withDefaultValue(magic_enum::enum_name(LazyModeOptions::Off))
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties, std::to_array<core::PropertyReference>({
      NodeIDType,
      NodeID,
      NameSpaceIndex,
      MaxDepth,
      Lazy
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully retrieved OPC-UA nodes"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Retrieved OPC-UA nodes where value cannot be extracted (only if enabled)"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto NodeIDAttr = core::OutputAttributeDefinition<>{"NodeID", { Success }, "ID of the node."};
  EXTENSIONAPI static constexpr auto NodeIDTypeAttr = core::OutputAttributeDefinition<>{"NodeID type", { Success }, "Type of the node ID."};
  EXTENSIONAPI static constexpr auto BrowsenameAttr = core::OutputAttributeDefinition<>{"Browsename", { Success }, "The browse name of the node."};
  EXTENSIONAPI static constexpr auto FullPathAttr = core::OutputAttributeDefinition<>{"Full path", { Success }, "The full path of the node."};
  EXTENSIONAPI static constexpr auto SourcetimestampAttr = core::OutputAttributeDefinition<>{"Sourcetimestamp", { Success },
    "The timestamp of when the node was created in the server as 'MM-dd-yyyy HH:mm:ss.mmm'."};
  EXTENSIONAPI static constexpr auto TypenameAttr = core::OutputAttributeDefinition<>{"Typename", { Success }, "The type name of the node data."};
  EXTENSIONAPI static constexpr auto DatasizeAttr = core::OutputAttributeDefinition<>{"Datasize", { Success }, "The size of the node data."};

  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 7> {NodeIDAttr, NodeIDTypeAttr, BrowsenameAttr, FullPathAttr, SourcetimestampAttr,
    TypenameAttr, DatasizeAttr};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  bool nodeFoundCallBack(const UA_ReferenceDescription *ref, const std::string& path,
                         core::ProcessContext& context, core::ProcessSession& session,
                         size_t& nodes_found, size_t& variables_found, std::unordered_map<std::string, std::string>& state_map);
  void OPCData2FlowFile(const opc::NodeData& opc_node, core::ProcessContext& context, core::ProcessSession& session) const;
  void writeFlowFileUsingLazyModeWithTimestamp(const opc::NodeData& nodedata, core::ProcessContext& context, core::ProcessSession& session, size_t& variables_found,
    std::unordered_map<std::string, std::string>& state_map) const;
  void writeFlowFileUsingLazyModeWithNewValue(const opc::NodeData& nodedata, core::ProcessContext& context, core::ProcessSession& session, size_t& variables_found,
    std::unordered_map<std::string, std::string>& state_map) const;

  uint64_t max_depth_ = 0;
  LazyModeOptions lazy_mode_ = LazyModeOptions::Off;
  std::vector<UA_NodeId> translated_node_ids_;  // Only used when user provides path, path->nodeid translation is only done once
  core::StateManager* state_manager_ = nullptr;
};

}  // namespace org::apache::nifi::minifi::processors
