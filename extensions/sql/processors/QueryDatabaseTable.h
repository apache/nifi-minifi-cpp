/**
 * @file QueryDatabaseTable.h
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
#include <vector>
#include <unordered_map>
#include <memory>

#include "core/ProcessSession.h"
#include "SQLProcessor.h"
#include "FlowFileSource.h"
#include "data/SQLColumnIdentifier.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors {

class QueryDatabaseTable: public SQLProcessor, public FlowFileSource {
 public:
  explicit QueryDatabaseTable(std::string name, const utils::Identifier& uuid = {});

  EXTENSIONAPI static const std::string RESULT_TABLE_NAME;
  EXTENSIONAPI static const std::string RESULT_ROW_COUNT;
  EXTENSIONAPI static const std::string TABLENAME_KEY;
  EXTENSIONAPI static const std::string MAXVALUE_KEY_PREFIX;
  EXTENSIONAPI static const std::string InitialMaxValueDynamicPropertyPrefix;

  EXTENSIONAPI static constexpr const char* Description = "QueryDatabaseTable to execute SELECT statement via ODBC.";

  EXTENSIONAPI static const core::Property TableName;
  EXTENSIONAPI static const core::Property ColumnNames;
  EXTENSIONAPI static const core::Property MaxValueColumnNames;
  EXTENSIONAPI static const core::Property WhereClause;
  static auto properties() {
    return utils::array_cat(SQLProcessor::properties(), FlowFileSource::properties(), std::array{
      TableName,
      ColumnNames,
      MaxValueColumnNames,
      WhereClause
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;

 private:
  std::string buildSelectQuery();

  void initializeMaxValues(core::ProcessContext& context);
  bool loadMaxValuesFromStoredState(const std::unordered_map<std::string, std::string>& state);
  void loadMaxValuesFromDynamicProperties(core::ProcessContext& context);

  bool saveState();

  core::CoreComponentStateManager* state_manager_;
  std::string table_name_;
  std::unordered_set<sql::SQLColumnIdentifier> return_columns_;
  std::string queried_columns_;
  std::string extra_where_clause_;
  std::vector<sql::SQLColumnIdentifier> max_value_columns_;
  std::unordered_map<sql::SQLColumnIdentifier, std::string> max_values_;
};

}  // namespace org::apache::nifi::minifi::processors
