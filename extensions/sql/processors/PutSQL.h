/**
 * @file PutSQL.h
 * PutSQL class declaration
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
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "SQLProcessor.h"
#include "utils/ArrayUtils.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::processors {

class PutSQL : public SQLProcessor {
 public:
  using SQLProcessor::SQLProcessor;

  EXTENSIONAPI static constexpr const char* Description =
      "Executes a SQL UPDATE or INSERT command. The content of an incoming FlowFile is expected to be the SQL command to execute. "
      "The SQL command may use the ? character to bind parameters. In this case, the parameters to use must exist as FlowFile attributes with the naming convention "
      "sql.args.N.type and sql.args.N.value, where N is a positive integer. The content of the FlowFile is expected to be in UTF-8 format.";

  EXTENSIONAPI static constexpr auto SQLStatement = core::PropertyDefinitionBuilder<>::createProperty("SQL Statement")
      .withDescription(
        "The SQL statement to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. "
        "If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of "
        "the incoming flow file is expected to contain a valid SQL statement, to be issued by the processor to the database.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SQLProcessor::Properties, std::to_array<core::PropertyReference>({SQLStatement}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "After a successful SQL update operation, the incoming FlowFile sent here"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Flow files that contain malformed sql statements"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;
};

}  // namespace org::apache::nifi::minifi::processors
