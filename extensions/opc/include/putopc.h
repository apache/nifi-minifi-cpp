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
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::processors {

class PutOPCProcessor : public BaseOPCProcessor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Creates/updates  OPC nodes";

  EXTENSIONAPI static constexpr auto ParentNodeIDType = core::PropertyDefinitionBuilder<3>::createProperty("Parent node ID type")
      .withDescription("Specifies the type of the provided node ID")
      .isRequired(true)
      .withAllowedValues({"Path", "Int", "String"})
      .build();
  EXTENSIONAPI static constexpr auto ParentNodeID = core::PropertyDefinitionBuilder<>::createProperty("Parent node ID")
      .withDescription("Specifies the ID of the root node to traverse")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ParentNameSpaceIndex = core::PropertyDefinitionBuilder<>::createProperty("Parent node namespace index")
      .withDescription("The index of the namespace. Used only if node ID type is not path.")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto ValueType = core::PropertyDefinitionBuilder<opc::StringToOPCDataTypeMap.size()>::createProperty("Value type")
      .withDescription("Set the OPC value type of the created nodes")
      .withAllowedValues(utils::getKeys(opc::StringToOPCDataTypeMap))
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto TargetNodeIDType = core::PropertyDefinitionBuilder<>::createProperty("Target node ID type")
      .withDescription("ID type of target node. Allowed values are: Int, String.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto TargetNodeID = core::PropertyDefinitionBuilder<>::createProperty("Target node ID")
      .withDescription("ID of target node.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto TargetNodeBrowseName = core::PropertyDefinitionBuilder<>::createProperty("Target node browse name")
      .withDescription("Browse name of target node. Only used when new node is created.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto TargetNodeNameSpaceIndex = core::PropertyDefinitionBuilder<>::createProperty("Target node namespace index")
      .withDescription("The index of the namespace. Used only if node ID type is not path.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties, std::to_array<core::PropertyReference>({
      ParentNodeIDType,
      ParentNodeID,
      ParentNameSpaceIndex,
      ValueType,
      TargetNodeIDType,
      TargetNodeID,
      TargetNodeBrowseName,
      TargetNodeNameSpaceIndex
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully put OPC-UA node"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failed to put OPC-UA node"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutOPCProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : BaseOPCProcessor(name, uuid) {
    logger_ = core::logging::LoggerFactory<PutOPCProcessor>::getLogger(uuid_);
  }

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::string nodeID_;
  int32_t nameSpaceIdx_{};
  opc::OPCNodeIDType idType_{};
  UA_NodeId parentNodeID_{};
  bool parentExists_{};
  opc::OPCNodeDataType nodeDataType_{};
};

}  // namespace org::apache::nifi::minifi::processors
