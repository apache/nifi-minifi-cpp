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

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BaseOPCProcessor.h"
#include "OPCCommon.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/controllers/RecordSetWriter.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/core/StateManager.h"

namespace org::apache::nifi::minifi::processors {

struct FetchedState {
  int64_t timestamp = 0;
  std::unordered_set<std::string> fingerprints;
};

struct FetchOPCHistoryContext {
  core::ProcessSession& session;
  std::shared_ptr<core::RecordSetWriter> record_set_writer;
  std::unordered_map<std::string, std::string>& state_map;
  size_t& entries_transferred;
  const uint64_t batch_size;
  const std::string& node_id;
  const int32_t namespace_index;
  const std::optional<FetchedState> fetched_state;
  std::shared_ptr<core::logging::Logger> logger;
};

class FetchOPCHistory final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description =
      "Fetches OPC-UA node history between the start and end timestamps. "
      "A history entry is only fetched once, on every trigger only the not yet fetched entries are returned.";

  EXTENSIONAPI static constexpr auto NodeIDType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Node ID type")
          .withDescription("Specifies the type of the provided node ID")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
          .build();
  EXTENSIONAPI static constexpr auto NodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Node ID")
          .withDescription(
              "Specifies the ID of the root node to fetch history for. "
              "In case of a Path Node ID Type, the path should be provided in the format of 'path/to/node'.")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
          .withDescription("The index of the namespace.")
          .withValidator(core::StandardPropertyValidators::INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto StartTimestamp =
      core::PropertyDefinitionBuilder<>::createProperty("Start timestamp")
          .withDescription(
              "Timestamp after which the events should be returned. If not specified entries are returned from the beginning of the history.")
          .build();
  EXTENSIONAPI static constexpr auto EndTimestamp =
      core::PropertyDefinitionBuilder<>::createProperty("End timestamp")
          .withDescription("Timestamp before which the events should be returned. If not specified entries are returned until the current time.")
          .build();
  EXTENSIONAPI static constexpr auto BatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
          .withDescription("Maximum number entries to read and return in a single batch. If set to zero or empty all available entries are returned.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .build();
  EXTENSIONAPI static constexpr auto HistoryReadType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::HistoryReadTypeOption>()>::createProperty("History Read Type")
          .withDescription("Whether to fetch raw historical values or the audit trail of modifications to historical values")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<opc::HistoryReadTypeOption>())
          .withDefaultValue(magic_enum::enum_name<opc::HistoryReadTypeOption::Raw>())
          .build();
  EXTENSIONAPI static constexpr auto RecordSetWriter =
      core::PropertyDefinitionBuilder<>::createProperty("Record Set Writer")
          .withDescription("Specifies the Controller Service to use for writing results to a FlowFile instead of using the default output format.")
          .withAllowedTypes<core::RecordSetWriter>()
          .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties,
      std::to_array<core::PropertyReference>(
          {NodeIDType, NodeID, NameSpaceIndex, StartTimestamp, EndTimestamp, BatchSize, HistoryReadType, RecordSetWriter}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully retrieved OPC-UA node history entries"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto NodeIDAttr = core::OutputAttributeDefinition<>{"NodeID", {Success}, "ID of the node."};
  EXTENSIONAPI static constexpr auto NamespaceIndexAttr = core::OutputAttributeDefinition<>{
      "Namespace index", {Success}, "Namespace index of the node."};
  EXTENSIONAPI static constexpr auto SourcetimestampAttr = core::OutputAttributeDefinition<>{
      "Sourcetimestamp", {Success}, "The timestamp of when the node was created in the server as 'YYYY-MM-DDTHH:MM:SS.sssZ'."};
  EXTENSIONAPI static constexpr auto ModificationUsernameAttr = core::OutputAttributeDefinition<>{
      "ModificationUsername", {Success}, "Username of the user who modified the node."};
  EXTENSIONAPI static constexpr auto ModificationTimeAttr = core::OutputAttributeDefinition<>{
      "ModificationTime", {Success}, "Timestamp of when the node was modified."};
  EXTENSIONAPI static constexpr auto ModificationUpdateTypeAttr = core::OutputAttributeDefinition<>{
      "ModificationUpdateType", {Success}, "Type of modification performed on the node."};

  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 6>{
      NodeIDAttr, NamespaceIndexAttr, SourcetimestampAttr, ModificationUsernameAttr, ModificationTimeAttr, ModificationUpdateTypeAttr};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::optional<FetchedState> parseFetchedState(const std::unordered_map<std::string, std::string>& state_map);

  opc::HistoryReadTypeOption history_type_ = opc::HistoryReadTypeOption::Raw;
  std::optional<std::chrono::system_clock::time_point> start_timestamp_;
  std::optional<std::chrono::system_clock::time_point> end_timestamp_;
  uint64_t batch_size_ = 0;
  std::shared_ptr<core::RecordSetWriter> record_set_writer_;
  opc::NodeId node_;
  bool path_node_id_resolved_ = false;
};

}  // namespace org::apache::nifi::minifi::processors
