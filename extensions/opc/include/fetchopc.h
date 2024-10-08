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
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

class FetchOPCProcessor : public BaseOPCProcessor {
 public:
  explicit FetchOPCProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : BaseOPCProcessor(name, uuid) {
    logger_ = core::logging::LoggerFactory<FetchOPCProcessor>::getLogger(uuid_);
  }

  EXTENSIONAPI static constexpr const char* Description = "Fetches OPC-UA node";

  EXTENSIONAPI static constexpr auto NodeIDType = core::PropertyDefinitionBuilder<3>::createProperty("Node ID type")
      .withDescription("Specifies the type of the provided node ID")
      .isRequired(true)
      .withAllowedValues({"Path", "Int", "String"})
      .build();
  EXTENSIONAPI static constexpr auto NodeID = core::PropertyDefinitionBuilder<>::createProperty("Node ID")
      .withDescription("Specifies the ID of the root node to traverse")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex = core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
      .withDescription("The index of the namespace. Used only if node ID type is not path.")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto MaxDepth = core::PropertyDefinitionBuilder<>::createProperty("Max depth")
      .withDescription("Specifiec the max depth of browsing. 0 means unlimited.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto Lazy = core::PropertyDefinitionBuilder<2>::createProperty("Lazy mode")
      .withDescription("Only creates flowfiles from nodes with new timestamp from the server.")
      .withDefaultValue("Off")
      .isRequired(true)
      .withAllowedValues({"On", "Off"})
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

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 protected:
  bool nodeFoundCallBack(opc::Client& client, const UA_ReferenceDescription *ref, const std::string& path,
                         core::ProcessContext& context, core::ProcessSession& session);

  void OPCData2FlowFile(const opc::NodeData& opcnode, core::ProcessContext& context, core::ProcessSession& session);

  std::string nodeID_;
  int32_t nameSpaceIdx_ = 0;
  opc::OPCNodeIDType idType_{};
  uint32_t nodesFound_ = 0;
  uint32_t variablesFound_ = 0;
  uint64_t maxDepth_ = 0;
  bool lazy_mode_ = false;

 private:
  std::vector<UA_NodeId> translatedNodeIDs_;  // Only used when user provides path, path->nodeid translation is only done once
  std::unordered_map<std::string, std::string> node_timestamp_;  // Key = Full path, Value = Timestamp
};

}  // namespace org::apache::nifi::minifi::processors
