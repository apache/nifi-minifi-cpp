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
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/RelationshipDefinition.h"
#include "controllers/SSLContextServiceInterface.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

enum class LazyModeOptions {
  On,
  Off
};

class FetchOPCProcessor : public BaseOPCProcessor {
 public:
  explicit FetchOPCProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : BaseOPCProcessor(name, uuid) {
    logger_ = core::logging::LoggerFactory<FetchOPCProcessor>::getLogger(uuid_);
  }

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
      .withDescription("Only creates flowfiles from nodes with new timestamp from the server.")
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

 protected:
  bool nodeFoundCallBack(const UA_ReferenceDescription *ref, const std::string& path,
                         core::ProcessContext& context, core::ProcessSession& session,
                         size_t& nodes_found, size_t& variables_found);

  void OPCData2FlowFile(const opc::NodeData& opc_node, core::ProcessContext& context, core::ProcessSession& session);

  uint64_t max_depth_ = 0;
  bool lazy_mode_ = false;

 private:
  std::vector<UA_NodeId> translated_node_ids_;  // Only used when user provides path, path->nodeid translation is only done once
  std::unordered_map<std::string, std::string> node_timestamp_;  // Key = Full path, Value = Timestamp
};

}  // namespace org::apache::nifi::minifi::processors
