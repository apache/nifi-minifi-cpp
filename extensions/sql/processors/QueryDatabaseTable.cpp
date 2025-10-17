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

#include "QueryDatabaseTable.h"

#include <vector>
#include <string>
#include <algorithm>

#include "io/BufferStream.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/Exception.h"
#include "data/MaxCollector.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const std::string QueryDatabaseTable::InitialMaxValueDynamicPropertyPrefix("initial.maxvalue.");

const std::string QueryDatabaseTable::RESULT_TABLE_NAME = "tablename";
const std::string QueryDatabaseTable::RESULT_ROW_COUNT = "querydbtable.row.count";

const std::string QueryDatabaseTable::TABLENAME_KEY = "tablename";
const std::string QueryDatabaseTable::MAXVALUE_KEY_PREFIX = "maxvalue.";

void QueryDatabaseTable::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void QueryDatabaseTable::processOnSchedule(core::ProcessContext& context) {
  output_format_ = utils::parseEnumProperty<flow_file_source::OutputType>(context, OutputFormat);
  max_rows_ = gsl::narrow<size_t>(utils::parseU64Property(context, MaxRowsPerFlowFile));

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  table_name_ = context.getProperty(TableName).value_or("");
  extra_where_clause_ = context.getProperty(WhereClause).value_or("");

  return_columns_.clear();
  queried_columns_.clear();
  for (auto&& raw_col : utils::string::splitAndTrimRemovingEmpty(context.getProperty(ColumnNames).value_or(""), ",")) {
    if (!queried_columns_.empty()) {
      queried_columns_ += ", ";
    }
    queried_columns_ += raw_col;
    return_columns_.insert(sql::SQLColumnIdentifier(std::move(raw_col)));
  }

  max_value_columns_.clear();
  for (auto&& raw_col : utils::string::splitAndTrimRemovingEmpty(context.getProperty(MaxValueColumnNames).value_or(""), ",")) {
    sql::SQLColumnIdentifier col_id(raw_col);
    if (!queried_columns_.empty() && !return_columns_.contains(col_id)) {
      // columns will be explicitly enumerated, we need to add the max value columns as it is not yet queried
      queried_columns_ += ", ";
      queried_columns_ += raw_col;
    }
    max_value_columns_.push_back(std::move(col_id));
  }

  initializeMaxValues(context);
}

void QueryDatabaseTable::processOnTrigger(core::ProcessContext& /*context*/, core::ProcessSession& session) {
  const auto& selectQuery = buildSelectQuery();

  logger_->log_info("QueryDatabaseTable: selectQuery: '{}'", selectQuery.c_str());

  auto statement = connection_->prepareStatement(selectQuery);

  auto rowset = statement->execute();

  std::unordered_map<sql::SQLColumnIdentifier, std::string> new_max_values = max_values_;
  sql::MaxCollector maxCollector{selectQuery, new_max_values};
  auto column_filter = [&] (const std::string& column_name) {
    return return_columns_.empty() || return_columns_.contains(sql::SQLColumnIdentifier(column_name));
  };
  sql::JSONSQLWriter json_writer{output_format_ == flow_file_source::OutputType::JSONPretty, column_filter};
  FlowFileGenerator flow_file_creator{session, json_writer};
  sql::SQLRowsetProcessor sql_rowset_processor(std::move(rowset), {json_writer, maxCollector, flow_file_creator});

  while (size_t row_count = sql_rowset_processor.process(max_rows_)) {
    auto new_file = flow_file_creator.getLastFlowFile();
    gsl_Expects(new_file);
    new_file->addAttribute(RESULT_ROW_COUNT, std::to_string(row_count));
    new_file->addAttribute(RESULT_TABLE_NAME, table_name_);
  }

  // the updated max_values and the total number of flow_files is available from here
  for (auto& new_file : flow_file_creator.getFlowFiles()) {
    session.transfer(new_file, Success);
    for (const auto& max_column : max_value_columns_) {
      new_file->addAttribute("maxvalue." + max_column.str(), new_max_values[max_column]);
    }
  }

  if (new_max_values != max_values_) {
    max_values_ = new_max_values;
    saveState();
  }
}

bool QueryDatabaseTable::loadMaxValuesFromStoredState(const std::unordered_map<std::string, std::string> &state) {
  std::unordered_map<sql::SQLColumnIdentifier, std::string> new_max_values;
  if (!state.contains(TABLENAME_KEY)) {
    logger_->log_info("State does not specify the table name.");
    return false;
  }
  if (state.at(TABLENAME_KEY) != table_name_) {
    logger_->log_info("Querying new table \"{}\", resetting state.", table_name_);
    return false;
  }
  for (auto& elem : state) {
    if (utils::string::startsWith(elem.first, MAXVALUE_KEY_PREFIX)) {
      sql::SQLColumnIdentifier column_name(elem.first.substr(MAXVALUE_KEY_PREFIX.length()));
      // add only those columns that we care about
      if (std::find(max_value_columns_.begin(), max_value_columns_.end(), column_name) != max_value_columns_.end()) {
        new_max_values.emplace(column_name, elem.second);
      } else {
        logger_->log_info("State contains obsolete maximum-value column \"{}\", resetting state.", column_name.str());
        return false;
      }
    }
  }
  for (auto& column : max_value_columns_) {
    if (!new_max_values.contains(column)) {
      logger_->log_info("New maximum-value column \"{}\" specified, resetting state.", column.str());
      return false;
    }
  }
  max_values_ = new_max_values;
  return true;
}

void QueryDatabaseTable::initializeMaxValues(core::ProcessContext &context) {
  max_values_.clear();
  std::unordered_map<std::string, std::string> stored_state;
  if (!state_manager_->get(stored_state)) {
    logger_->log_info("Found no stored state");
  } else {
    if (!loadMaxValuesFromStoredState(stored_state)) {
      state_manager_->clear();
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
  logger_->log_info("Received {} dynamic properties", dynamic_prop_keys.size());

  for (const auto& key : dynamic_prop_keys) {
    if (!utils::string::startsWith(key, InitialMaxValueDynamicPropertyPrefix)) {
      throw minifi::Exception(PROCESSOR_EXCEPTION, "QueryDatabaseTable: Unsupported dynamic property \"" + key + "\"");
    }
    sql::SQLColumnIdentifier column_name(key.substr(InitialMaxValueDynamicPropertyPrefix.length()));
    auto it = max_values_.find(column_name);
    if (it == max_values_.end()) {
      logger_->log_warn("Initial maximum value specified for column \"{}\", which is not specified as a Maximum-value Column. Ignoring.", column_name.str());
      continue;
    }
    // do not overwrite existing max value
    if (!it->second.empty()) {
      continue;
    }
    if (auto dynamic_property_value = context.getDynamicProperty(key); dynamic_property_value && !dynamic_property_value->empty()) {
      it->second = *dynamic_property_value;
      logger_->log_info("Setting initial maximum value of {} to {}", column_name.str(), *dynamic_property_value);
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
    where_clauses.push_back(utils::string::join_pack(column_name.str(), index == 0 ? " > " : " >= ", max_value));
  }

  if (!extra_where_clause_.empty()) {
    where_clauses.push_back(extra_where_clause_);
  }

  if (!where_clauses.empty()) {
    query += " where " + utils::string::join(" and ", where_clauses);
  }

  return query;
}

bool QueryDatabaseTable::saveState() {
  std::unordered_map<std::string, std::string> state_map;
  state_map.emplace(TABLENAME_KEY, table_name_);
  for (const auto& item : max_values_) {
    state_map.emplace(MAXVALUE_KEY_PREFIX + item.first.str(), item.second);
  }
  return state_manager_->set(state_map);
}

REGISTER_RESOURCE(QueryDatabaseTable, Processor);

}  // namespace org::apache::nifi::minifi::processors
