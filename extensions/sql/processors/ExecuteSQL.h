/**
 * @file ExecuteSQL.h
 * ExecuteSQL class declaration
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

#include <string>

#include "core/ProcessSession.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "SQLProcessor.h"
#include "FlowFileSource.h"
#include "utils/ArrayUtils.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::processors {

class ExecuteSQL : public SQLProcessor, public FlowFileSource {
 public:
  using SQLProcessor::SQLProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Execute provided SQL query. "
      "Query result rows will be outputted as new flow files with attribute keys equal to result column names and values equal to result values. "
      "There will be one output FlowFile per result row. "
      "This processor can be scheduled to run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. "
      "If it is triggered by an incoming FlowFile, then attributes of that FlowFile will be available when evaluating the query.";

  EXTENSIONAPI static constexpr auto SQLSelectQuery = core::PropertyDefinitionBuilder<>::createProperty("SQL select query")
      .withDescription(
        "The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. "
        "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
        "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. "
        "Note that Expression Language is not evaluated for flow file contents.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SQLProcessor::Properties, FlowFileSource::Properties, std::to_array<core::PropertyReference>({SQLSelectQuery}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully created FlowFile from SQL query result set."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Flow files containing malformed sql statements"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;

  EXTENSIONAPI static const std::string RESULT_ROW_COUNT;
  EXTENSIONAPI static const std::string INPUT_FLOW_FILE_UUID;
};

}  // namespace org::apache::nifi::minifi::processors
