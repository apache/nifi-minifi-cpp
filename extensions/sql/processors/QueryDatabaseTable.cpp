/**
 * @file QueryDatabaseTable.cpp
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

#include "QueryDatabaseTable.h"

#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <cstdio>
#include <string>
#include <iostream>
#include <memory>
#include <codecvt>
#include <algorithm>
#include <regex>

#include <soci/soci.h>

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"
#include "utils/OsUtils.h"
#include "data/DatabaseConnectors.h"
#include "data/JSONSQLWriter.h"
#include "data/SQLRowsetProcessor.h"
#include "data/WriteCallback.h"
#include "data/MaxCollector.h"
#include "data/Utils.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string QueryDatabaseTable::ProcessorName("QueryDatabaseTable");

const core::Property QueryDatabaseTable::TableName(
  core::PropertyBuilder::createProperty("Table Name")
  ->isRequired(true)
  ->withDescription("The name of the database table to be queried.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::ColumnNames(
  core::PropertyBuilder::createProperty("Columns to Return")
  ->isRequired(false)
  ->withDescription(
    "A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. "
    "If no column names are supplied, all columns in the specified table will be returned. "
    "NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::MaxValueColumnNames(
  core::PropertyBuilder::createProperty("Maximum-value Columns")
  ->isRequired(false)
  ->withDescription(
    "A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. "
    "Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. "
    "Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. "
    "This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. "
    "Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, and will result in error(s) during processing. "
    "If no columns are provided, all rows from the table will be considered, which could have a performance impact. "
    "NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly. "
    "NOTE: Because of a limitation of database access library 'soci', which doesn't support milliseconds in it's 'dt_date', "
    "there is a possibility that flowfiles might have duplicated records, if a max-value column with 'dt_date' type has value with milliseconds.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::WhereClause(
  core::PropertyBuilder::createProperty("Where Clause")
  ->isRequired(false)
  ->withDescription("A custom clause to be added in the WHERE condition when building SQL queries.")
  ->supportsExpressionLanguage(true)->build());

const std::string QueryDatabaseTable::InitialMaxValueDynamicPropertyPrefix("initial.maxvalue.");

const core::Relationship QueryDatabaseTable::Success("success", "Successfully created FlowFile from SQL query result set.");

const std::string QueryDatabaseTable::RESULT_TABLE_NAME = "tablename";
const std::string QueryDatabaseTable::RESULT_ROW_COUNT = "querydbtable.row.count";

const std::string QueryDatabaseTable::TABLENAME_KEY = "tablename";
const std::string QueryDatabaseTable::MAXVALUE_KEY_PREFIX = "maxvalue.";

// QueryDatabaseTable
QueryDatabaseTable::QueryDatabaseTable(const std::string& name, utils::Identifier uuid)
  : SQLProcessor(name, uuid, logging::LoggerFactory<QueryDatabaseTable>::getLogger()) {
}

void QueryDatabaseTable::initialize() {
  //! Set the supported properties
  setSupportedProperties({
    DBControllerService, OutputFormat, TableName, ColumnNames,
    MaxValueColumnNames, WhereClause, MaxRowsPerFlowFile});

  //! Set the supported relationships
  setSupportedRelationships({ Success });
}

void QueryDatabaseTable::processOnSchedule(core::ProcessContext& context) {
  context.getProperty(OutputFormat.getName(), output_format_);
  max_rows_ = [&] {
    uint64_t max_rows;
    context.getProperty(MaxRowsPerFlowFile.getName(), max_rows);
    return gsl::narrow<size_t>(max_rows);
  }();

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  context.getProperty(TableName.getName(), table_name_);
  queried_columns_.clear();
  std::string return_columns_str;
  context.getProperty(ColumnNames.getName(), return_columns_str);
  auto return_columns = utils::inputStringToList(return_columns_str);
  std::set<std::string> queried_columns{return_columns.begin(), return_columns.end()};
  context.getProperty(WhereClause.getName(), extra_where_clause_);
  std::string max_value_columns_str;
  context.getProperty(MaxValueColumnNames.getName(), max_value_columns_str);
  max_value_columns_ = utils::inputStringToList(max_value_columns_str);
  if (!queried_columns.empty()) {
    queried_columns.insert(max_value_columns_.begin(), max_value_columns_.end());
  }
  queried_columns_ = utils::StringUtils::join(", ", queried_columns);

  initializeMaxValues(context);
}

void QueryDatabaseTable::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const auto& selectQuery = buildSelectQuery();

  logger_->log_info("QueryDatabaseTable: selectQuery: '%s'", selectQuery.c_str());

  auto statement = connection_->prepareStatement(selectQuery);

  auto rowset = statement->execute();

  std::unordered_map<std::string, std::string> new_max_values = max_values_;
  sql::MaxCollector maxCollector{selectQuery, new_max_values};
  sql::JSONSQLWriter sqlWriter{output_format_ == OutputType::JSONPretty};
  FlowFileGenerator flow_file_creator{session, sqlWriter};
  sql::SQLRowsetProcessor sqlRowsetProcessor(rowset, {sqlWriter, maxCollector, flow_file_creator});

  while (size_t row_count = sqlRowsetProcessor.process(max_rows_)) {
    auto new_file = flow_file_creator.getLastFlowFile();
    new_file->addAttribute(RESULT_ROW_COUNT, std::to_string(row_count));
    new_file->addAttribute(RESULT_TABLE_NAME, table_name_);
  }

  // the updated max_values and the total number of flow_files is available from here
  for (auto& new_file : flow_file_creator.getFlowFiles()) {
    session.transfer(new_file, Success);
    for (const auto& max_column : max_value_columns_) {
      new_file->addAttribute("maxvalue." + max_column, new_max_values[max_column]);
    }
  }

  if (new_max_values != max_values_) {
    try {
      session.commit();
    } catch (std::exception& e) {
      throw;
    }

    max_values_ = new_max_values;
    saveState();
  }
}

void QueryDatabaseTable::initializeMaxValues(core::ProcessContext &context) {
  max_values_.clear();
  std::unordered_map<std::string, std::string> new_state;
  if (!state_manager_->get(new_state)) {
    logger_->log_info("Found no stored state");
  } else {
    const bool should_reset_state = [&] {
      if (new_state[TABLENAME_KEY] != table_name_) {
        logger_->log_info("Querying new table \"%s\", resetting state.", table_name_);
        return true;
      }
      for (auto &&elem : new_state) {
        if (utils::StringUtils::startsWith(elem.first, MAXVALUE_KEY_PREFIX)) {
          std::string column_name = elem.first.substr(MAXVALUE_KEY_PREFIX.length());
          // add only those columns that we care about
          if (std::find(max_value_columns_.begin(), max_value_columns_.end(), column_name) != max_value_columns_.end()) {
            max_values_.emplace(column_name, std::move(elem.second));
          } else {
            logger_->log_info("State contains obsolete maximum-value column \"%s\", resetting state.", column_name);
            return true;
          }
        }
      }
      for (auto& column : max_value_columns_) {
        if (max_values_.find(column) == max_values_.end()) {
          logger_->log_info("New maximum-value column \"%s\" specified, resetting state.", column);
          return true;
        }
      }
      return false;
    }();
    if (should_reset_state) {
      state_manager_->clear();
      max_values_.clear();
    }
  }

  for (const auto& column_name : max_value_columns_) {
    // initialize column values
    max_values_[column_name];
  }

  loadMaxValuesFromDynamicProperties(context);
}

void QueryDatabaseTable::loadMaxValuesFromDynamicProperties(core::ProcessContext &context) {
  const auto dynamic_prop_keys = context.getDynamicPropertyKeys();
  logger_->log_info("Received %zu dynamic properties", dynamic_prop_keys.size());

  for (const auto& key : dynamic_prop_keys) {
    if (!utils::StringUtils::startsWith(key, InitialMaxValueDynamicPropertyPrefix)) {
      throw minifi::Exception(PROCESSOR_EXCEPTION, "QueryDatabaseTable: Unsupported dynamic property \"" + key + "\"");
    }
    const auto column_name = utils::StringUtils::toLower(key.substr(InitialMaxValueDynamicPropertyPrefix.length()));
    auto it = max_values_.find(column_name);
    if (it == max_values_.end()) {
      logger_->log_warn("Initial maximum value specified for column \"%s\", which is not specified as a Maximum-value Column. Ignoring.", column_name);
      continue;
    }
    // do not overwrite existing max value
    if (!it->second.empty()) {
      continue;
    }
    std::string value;
    if (context.getDynamicProperty(key, value) && !value.empty()) {
      it->second = value;
      logger_->log_info("Setting initial maximum value of %s to %s", column_name, value);
    }
  }
}

std::string QueryDatabaseTable::buildSelectQuery() {
  std::string query = "select " + (queried_columns_.empty() ? "*" : queried_columns_) + " from " + table_name_;

  std::vector<std::string> where_clauses;

  for (size_t index = 0; index < max_value_columns_.size(); index++) {
    const auto& column_name = max_value_columns_[index];
    const auto& max_value = max_values_[column_name];
    if (max_value.empty()) {
      // max value has not been set for this column
      continue;
    }

    // Logic to differentiate ">" vs ">=" based on index is copied from:
    // https://github.com/apache/nifi/blob/master/nifi-nar-bundles/nifi-standard-bundle/nifi-standard-processors/src/main/java/org/apache/nifi/processors/standard/AbstractQueryDatabaseTable.java
    // (under comment "Add a condition for the WHERE clause"). And implementation explanation: https://issues.apache.org/jira/browse/NIFI-2712.
    where_clauses.push_back(utils::StringUtils::join_pack(column_name, index == 0 ? " > " : " >= ", max_value));
  }

  if (!extra_where_clause_.empty()) {
    where_clauses.push_back(extra_where_clause_);
  }

  if (!where_clauses.empty()) {
    query += " where " + utils::StringUtils::join(" and ", where_clauses);
  }

  return query;
}

bool QueryDatabaseTable::saveState() {
  std::unordered_map<std::string, std::string> state_map;
  state_map.emplace(TABLENAME_KEY, table_name_);
  for (const auto& item : max_values_) {
    state_map.emplace(MAXVALUE_KEY_PREFIX + item.first, item.second);
  }
  return state_manager_->set(state_map);
}


}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
