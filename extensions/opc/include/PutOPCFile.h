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
#include <optional>
#include <string>

#include "BaseOPCProcessor.h"
#include "OPCCommon.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::processors {

enum class OPCFileWriteMode {
  Overwrite,
  Append
};

class PutOPCFile final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description =
      "Writes the content of the flowfile to an OPC UA FileType node using its Open, Write and Close methods. "
      "If the target file node exists it is updated. Otherwise the file is looked up by its browse name in the parent FileDirectoryType node, "
      "and created there using the CreateFile method if it does not exist. The node ID of a created file is assigned by the server.";

  EXTENSIONAPI static constexpr auto TargetNodeIDType =
      core::PropertyDefinitionBuilder<>::createProperty("Target node ID type")
          .withDescription("ID type of the target file node. Allowed values are: Path, Int, String, Guid. Required if Target node ID is set.")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto TargetNodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Target node ID")
          .withDescription(
              "ID of an existing target file node to update. If the node does not exist, the file is looked up or created in the parent node using "
              "the target node browse name, except for the Path Node ID Type, where the node must exist. In case of a Path Node ID Type, the path "
              "should be provided in the format of 'path/to/node'.")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto TargetNodeNameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Target node namespace index")
          .withDescription(
              "The index of the namespace of the target file node. In case of a Path Node ID Type, it is used for every node in the path.")
          .supportsExpressionLanguage(true)
          .withDefaultValue("0")
          .build();
  EXTENSIONAPI static constexpr auto TargetPathReferenceTypes =
      core::PropertyDefinitionBuilder<>::createProperty("Target node path reference types")
          .withDescription(
              "Specify the reference types between nodes in the target path if Path Target node ID type is used. If not provided, all reference "
              "types are assumed to be Organizes. The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference "
              "types are Organizes, HasComponent, HasProperty, and HasSubtype.")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto TargetNodeBrowseName =
      core::PropertyDefinitionBuilder<>::createProperty("Target node browse name")
          .withDescription(
              "Browse name of the target file node in the parent node. Used to look up the file in the parent node, and to create it there if it "
              "does not exist. Required if the target file node is not found by its node ID.")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto ParentNodeIDType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Parent node ID type")
          .withDescription("Specifies the type of the provided parent node ID.")
          .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
          .build();
  EXTENSIONAPI static constexpr auto ParentNodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Parent node ID")
          .withDescription(
              "ID of the FileDirectoryType node containing the target file. Required if the target file node is not found by its node ID. "
              "In case of a Path Node ID Type, the path should be provided in the format of 'path/to/node'.")
          .build();
  EXTENSIONAPI static constexpr auto ParentNameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Parent node namespace index")
          .withDescription("The index of the namespace of the parent node.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .build();
  EXTENSIONAPI static constexpr auto WriteMode =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<OPCFileWriteMode>()>::createProperty("Write mode")
          .withDescription("Overwrite replaces the existing content of the file, Append adds the flowfile content to the end of the file.")
          .withAllowedValues(magic_enum::enum_names<OPCFileWriteMode>())
          .withDefaultValue(magic_enum::enum_name(OPCFileWriteMode::Overwrite))
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto PathReferenceTypes = core::PropertyDefinitionBuilder<>::createProperty("Path reference types")
      .withDescription("Specify the reference types between nodes in the parent node path if Parent node ID type is Path. If not provided, all reference types are assumed to be Organizes. "
                       "The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference types are Organizes, HasComponent, HasProperty, and HasSubtype. "
                       "Subtypes of the specified reference types are also matched.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties,
      std::to_array<core::PropertyReference>({TargetNodeIDType,
          TargetNodeID,
          TargetNodeNameSpaceIndex,
          TargetPathReferenceTypes,
          TargetNodeBrowseName,
          ParentNodeIDType,
          ParentNodeID,
          ParentNameSpaceIndex,
          PathReferenceTypes,
          WriteMode}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully written OPC UA file"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failed to write OPC UA file"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto NodeIDAttr = core::OutputAttributeDefinition<>{"NodeID", {Success}, "ID of the written file node."};
  EXTENSIONAPI static constexpr auto NodeIDTypeAttr = core::OutputAttributeDefinition<>{
      "NodeID type", {Success}, "Type of the node ID of the written file node."};
  EXTENSIONAPI static constexpr auto NamespaceIndexAttr = core::OutputAttributeDefinition<>{
      "Namespace index", {Success}, "Namespace index of the written file node."};
  EXTENSIONAPI static constexpr auto BrowsenameAttr = core::OutputAttributeDefinition<>{
      "Browsename", {Success}, "The browse name of the written file node."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{
      NodeIDAttr, NodeIDTypeAttr, NamespaceIndexAttr, BrowsenameAttr};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::expected<opc::NodeId, std::string> getTargetFileNode(core::ProcessContext& context, const core::FlowFile& flow_file);
  std::expected<opc::NodeId, std::string> findOrCreateFileInParent(core::ProcessContext& context, const core::FlowFile& flow_file);
  void addFileNodeAttributes(const UA_NodeId& file_node_id, core::FlowFile& flow_file) const;
  std::expected<void, std::string> writeFile(const UA_NodeId& file_node_id, core::ProcessSession& session,
      const std::shared_ptr<core::FlowFile>& flow_file) const;

  bool target_node_id_defined_ = false;
  bool parent_node_defined_ = false;
  OPCFileWriteMode write_mode_ = OPCFileWriteMode::Overwrite;
};

}  // namespace org::apache::nifi::minifi::processors
