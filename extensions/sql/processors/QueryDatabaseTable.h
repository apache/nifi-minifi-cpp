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


#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FlowFileSource.h"
#include "SQLProcessor.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/StateManager.h"
#include "data/SQLColumnIdentifier.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors {

class QueryDatabaseTable: public SQLProcessor, public FlowFileSource {
 public:
  using SQLProcessor::SQLProcessor;

  EXTENSIONAPI static const std::string RESULT_TABLE_NAME;
  EXTENSIONAPI static const std::string RESULT_ROW_COUNT;
  EXTENSIONAPI static const std::string TABLENAME_KEY;
  EXTENSIONAPI static const std::string MAXVALUE_KEY_PREFIX;
  EXTENSIONAPI static const std::string InitialMaxValueDynamicPropertyPrefix;

  EXTENSIONAPI static constexpr const char* Description =
      "Fetches all rows of a table, whose values in the specified Maximum-value Columns are larger than the previously-seen maxima. "
      "If that property is not provided, all rows are returned. The rows are grouped according to the value of Max Rows Per Flow File property and formatted as JSON.";

  EXTENSIONAPI static constexpr auto TableName = core::PropertyDefinitionBuilder<>::createProperty("Table Name")
      .withDescription("The name of the database table to be queried.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ColumnNames = core::PropertyDefinitionBuilder<>::createProperty("Columns to Return")
      .withDescription(
        "A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. "
        "If no column names are supplied, all columns in the specified table will be returned. "
        "NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxValueColumnNames = core::PropertyDefinitionBuilder<>::createProperty("Maximum-value Columns")
      .withDescription(
        "A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. "
        "Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. "
        "Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. "
        "This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. "
        "Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, "
        "and will result in error(s) during processing. "
        "If no columns are provided, all rows from the table will be considered, which could have a performance impact. "
        "NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly. "
        "NOTE: Because of a limitation of database access library 'soci', which doesn't support milliseconds in it's 'dt_date', "
        "there is a possibility that flowfiles might have duplicated records, if a max-value column with 'dt_date' type has value with milliseconds.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto WhereClause = core::PropertyDefinitionBuilder<>::createProperty("Where Clause")
      .withDescription("A custom clause to be added in the WHERE condition when building SQL queries.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SQLProcessor::Properties, FlowFileSource::Properties, std::to_array<core::PropertyReference>({
      TableName,
      ColumnNames,
      MaxValueColumnNames,
      WhereClause
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully created FlowFile from SQL query result set."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr auto InitialMaxValue = core::DynamicPropertyDefinition{"initial.maxvalue.<max_value_column>",
      "Initial maximum value for the specified column",
      "Specifies an initial max value for max value column(s). Properties should be added in the format `initial.maxvalue.<max_value_column>`. "
      "This value is only used the first time the table is accessed (when a Maximum Value Column is specified).",
      true};
  EXTENSIONAPI static constexpr auto DynamicProperties = std::array{InitialMaxValue};

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

  core::StateManager* state_manager_{};
  std::string table_name_;
  std::unordered_set<sql::SQLColumnIdentifier> return_columns_;
  std::string queried_columns_;
  std::string extra_where_clause_;
  std::vector<sql::SQLColumnIdentifier> max_value_columns_;
  std::unordered_map<sql::SQLColumnIdentifier, std::string> max_values_;
};

}  // namespace org::apache::nifi::minifi::processors
