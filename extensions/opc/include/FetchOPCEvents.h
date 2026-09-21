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
#include <mutex>
#include <optional>
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
#include "utils/ArrayUtils.h"
#include "utils/StoppableThread.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

class FetchOPCEvents final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description =
      "Subscribes to OPC-UA events from the specified node. "
      "The processor will continuously listen for events and write them to flow file records when the processor is triggered. "
      "The events can be filtered using the filtering criteria specified in the processor properties.";

  EXTENSIONAPI static constexpr auto NodeIDType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Node ID type")
          .withDescription("Specifies the type of the provided node ID")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
          .build();
  EXTENSIONAPI static constexpr auto NodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Node ID")
          .withDescription(
              "Specifies the node ID of the target OPC-UA node to subscribe to. In case of a Path Node ID Type, the path should be provided in the "
              "format of 'path/to/node'.")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
          .withDescription("Specifies the index of the namespace for the target OPC-UA node.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto EventTypeNodeId =
      core::PropertyDefinitionBuilder<>::createProperty("Event type node ID")
          .withDescription(
              "Adds event type to the event filter. Only events of this type or its subtypes are reported. "
              "i=2041 (BaseEventType) is inherited by every event type, so it applies no type filtering.")
          .withDefaultValue("i=2041")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto SelectFields =
      core::PropertyDefinitionBuilder<>::createProperty("Select fields")
          .withDescription(
              "Comma separated browse paths of the event fields to return, e.g. 'EventId,Time,Message,Severity,SourceName'. "
              "Nested paths use '/', e.g. 'Tool/Diameter', and a browse name outside namespace 0 is prefixed with its namespace "
              "index, e.g. '1:Diameter'. A path can be prefixed with the node ID of an event type to restrict the field to the "
              "instances of that type, e.g. 'i=2052/ActionTimeStamp', but the server rejects the whole subscription unless it "
              "knows that event type and the field belongs to it. Each field is named after its path in the record.")
          .withDefaultValue("EventId,EventType,SourceName,Time,Message,Severity")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto MinimumSeverity =
      core::PropertyDefinitionBuilder<>::createProperty("Minimum severity")
          .withDescription("Specifies the minimum severity of events to fetch. 0 disables the severity filter.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto EventFilterExpression =
      core::PropertyDefinitionBuilder<>::createProperty("Event filter expression")
          .withDescription(
              "Custom filter expression to apply to the OPC-UA events. It is an open62541 library specific query language syntax. "
              "This overrides the other filter properties (Event type node ID, Select fields, Minimum severity). "
              "Example: SELECT /Message, /Severity WHERE /Severity >= 100")
          .build();
  EXTENSIONAPI static constexpr auto MaxQueueSize =
      core::PropertyDefinitionBuilder<>::createProperty("Max size of message queue")
          .withDescription(
              "Maximum number of events allowed to be buffered before processing them when the processor is triggered. "
              "If the buffer is full, the oldest events are dropped. If not set or set to zero the buffer is unlimited.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("10000")
          .build();
  EXTENSIONAPI static constexpr auto BatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Batch size")
          .withDescription(
              "Maximum number of events to put in a single flow file. If set to zero or empty all events retrieved in a single trigger are put in a "
              "single flow file.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .build();
  EXTENSIONAPI static constexpr auto RecordSetWriter =
      core::PropertyDefinitionBuilder<>::createProperty("Record set writer")
          .withDescription("Specifies the Controller Service to use for writing results to a FlowFile.")
          .withAllowedTypes<core::RecordSetWriter>()
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties,
      std::to_array<core::PropertyReference>({NodeIDType,
          NodeID,
          NameSpaceIndex,
          EventTypeNodeId,
          SelectFields,
          MinimumSeverity,
          EventFilterExpression,
          MaxQueueSize,
          BatchSize,
          RecordSetWriter}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully retrieved OPC-UA events"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  ~FetchOPCEvents() override;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 protected:
  void notifyStop() override;

 private:
  void createFlowFiles(core::ProcessSession& session, const std::vector<opc::Event>& events) const;
  void runEventLoop();
  void stopEventThread();

  std::shared_ptr<core::RecordSetWriter> record_set_writer_;
  std::optional<uint64_t> batch_size_;
  opc::EventFilter event_filter_;
  std::mutex connection_mutex_;
  std::unique_ptr<utils::StoppableThread> event_thread_;
  std::chrono::milliseconds retry_interval_ = 1s;
};

}  // namespace org::apache::nifi::minifi::processors
