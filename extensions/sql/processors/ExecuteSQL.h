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
#include "core/ProcessContext.h"
#include "SQLProcessor.h"
#include "FlowFileSource.h"
#include "utils/ArrayUtils.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::processors {

class ExecuteSQL : public SQLProcessor, public FlowFileSource {
 public:
  explicit ExecuteSQL(std::string name, const utils::Identifier& uuid = {});

  EXTENSIONAPI static constexpr const char* Description = "Execute provided SQL query. "
      "Query result rows will be outputted as new flow files with attribute keys equal to result column names and values equal to result values. "
      "There will be one output FlowFile per result row. "
      "This processor can be scheduled to run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. "
      "If it is triggered by an incoming FlowFile, then attributes of that FlowFile will be available when evaluating the query.";

  EXTENSIONAPI static const core::Property SQLSelectQuery;
  static auto properties() {
    return utils::array_cat(SQLProcessor::properties(), FlowFileSource::properties(), std::array{SQLSelectQuery});
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

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
