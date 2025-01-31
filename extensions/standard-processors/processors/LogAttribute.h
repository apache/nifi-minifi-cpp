/**
 * @file LogAttribute.h
 * LogAttribute class declaration
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
#include <string>
#include <utility>
#include <vector>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class LogAttribute : public core::ProcessorImpl {
 public:
  explicit LogAttribute(const std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
    logger_->set_max_log_size(-1);
  }
  ~LogAttribute() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Logs attributes of flow files in the MiNiFi application log.";

  EXTENSIONAPI static constexpr auto LogLevel = core::PropertyDefinitionBuilder<6>::createProperty("Log Level")
      .withDescription("The Log Level to use when logging the Attributes")
      .withAllowedValues({"trace", "debug", "info", "warn", "error", "critical"})
      .build();
  EXTENSIONAPI static constexpr auto AttributesToLog = core::PropertyDefinitionBuilder<>::createProperty("Attributes to Log")
      .withDescription("A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.")
      .build();
  EXTENSIONAPI static constexpr auto AttributesToIgnore = core::PropertyDefinitionBuilder<>::createProperty("Attributes to Ignore")
      .withDescription("A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.")
      .build();
  EXTENSIONAPI static constexpr auto LogPayload = core::PropertyDefinitionBuilder<>::createProperty("Log Payload")
      .withDescription("If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto HexencodePayload = core::PropertyDefinitionBuilder<>::createProperty("Hexencode Payload")
      .withDescription("If true, the FlowFile's payload will be logged in a hexencoded format")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto MaxPayloadLineLength = core::PropertyDefinitionBuilder<>::createProperty("Maximum Payload Line Length")
      .withDescription("The logged payload will be broken into lines this long. 0 means no newlines will be added.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto LogPrefix = core::PropertyDefinitionBuilder<>::createProperty("Log Prefix")
      .withDescription("Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.")
      .build();
  EXTENSIONAPI static constexpr auto FlowFilesToLog = core::PropertyDefinitionBuilder<>::createProperty("FlowFiles To Log")
      .withDescription("Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      LogLevel,
      AttributesToLog,
      AttributesToIgnore,
      LogPayload,
      HexencodePayload,
      MaxPayloadLineLength,
      LogPrefix,
      FlowFilesToLog
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::string generateLogMessage(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) const;

  uint64_t flowfiles_to_log_{1};
  bool hexencode_{false};
  uint32_t max_line_length_{80};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LogAttribute>::getLogger(uuid_);
  core::logging::LOG_LEVEL log_level_{core::logging::LOG_LEVEL::info};
  std::string dash_line_ = "--------------------------------------------------";
  bool log_payload_ = false;
  std::optional<std::unordered_set<std::string>> attributes_to_log_;
  std::optional<std::unordered_set<std::string>> attributes_to_ignore_;
};

}  // namespace org::apache::nifi::minifi::processors
