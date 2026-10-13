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
#include "OPCFileTransfer.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/core/StateManager.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors {

enum class FetchOPCFileLazyModeOptions {
  On,
  Off
};

class FetchOPCFile final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Fetches the content of an OPC UA FileType node using its Open, Read and Close methods.";

  EXTENSIONAPI static constexpr auto NodeIDType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Node ID type")
          .withDescription("Specifies the type of the provided node ID")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
          .build();
  EXTENSIONAPI static constexpr auto NodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Node ID")
          .withDescription(
              "Specifies the ID of the file node to fetch. In case of a Path Node ID Type, the path should be provided in the format of "
              "'path/to/node'.")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
          .withDescription("The index of the namespace.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto Lazy =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<FetchOPCFileLazyModeOptions>()>::createProperty("Lazy mode")
          .withDescription(
              "If set to 'On', the file is only fetched if its LastModifiedTime or Size property has changed since the last fetch. "
              "If the file node has no LastModifiedTime property, only the changes of its Size are detected.")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<FetchOPCFileLazyModeOptions>())
          .withDefaultValue(magic_enum::enum_name<FetchOPCFileLazyModeOptions>(FetchOPCFileLazyModeOptions::Off))
          .build();
  EXTENSIONAPI static constexpr auto PathReferenceTypes =
      core::PropertyDefinitionBuilder<>::createProperty("Path reference types")
          .withDescription("Specify the reference types between nodes in the path if Path Node ID type is used. If not provided, all reference types are assumed to be Organizes. "
                           "The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference types are Organizes, HasComponent, HasProperty, and HasSubtype. "
                           "Subtypes of the specified reference types are also matched.")
          .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties,
      std::to_array<core::PropertyReference>({NodeIDType, NodeID, NameSpaceIndex, Lazy, PathReferenceTypes}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully fetched OPC UA file content"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto NodeIDAttr = core::OutputAttributeDefinition<>{"NodeID", {Success}, "ID of the file node."};
  EXTENSIONAPI static constexpr auto NodeIDTypeAttr = core::OutputAttributeDefinition<>{"NodeID type", {Success}, "Type of the node ID."};
  EXTENSIONAPI static constexpr auto NamespaceIndexAttr = core::OutputAttributeDefinition<>{
      "Namespace index", {Success}, "Namespace index of the file node."};
  EXTENSIONAPI static constexpr auto BrowsenameAttr = core::OutputAttributeDefinition<>{"Browsename", {Success}, "The browse name of the file node."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{
      NodeIDAttr, NodeIDTypeAttr, NamespaceIndexAttr, BrowsenameAttr};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 protected:
  bool reconnect() override;

 private:
  struct CachedFile {
    opc::RemoteFile remote_file;
    UA_Int32 chunk_size;
    std::optional<std::string> browse_name;
    std::optional<opc::NodeId> size_node_id;
    std::optional<opc::NodeId> last_modified_time_node_id;
  };

  bool resolvePath();
  bool resolveFile();
  void invalidateFile();
  std::optional<core::StateManager::State> readFileState();
  bool writeOPCFileToFlowFile(opc::RemoteFile& remote_file, opc::RemoteFileHandle file_handle, UA_Int32 chunk_size, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file);

  std::optional<CachedFile> cached_file_;
  FetchOPCFileLazyModeOptions lazy_mode_ = FetchOPCFileLazyModeOptions::Off;
};

}  // namespace org::apache::nifi::minifi::processors
