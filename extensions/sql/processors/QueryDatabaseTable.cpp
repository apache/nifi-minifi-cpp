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

#include "io/DataStream.h"
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string QueryDatabaseTable::ProcessorName("QueryDatabaseTable");

const core::Property QueryDatabaseTable::s_tableName(
  core::PropertyBuilder::createProperty("Table Name")->isRequired(true)->withDescription("The name of the database table to be queried.")->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_columnNames(
  core::PropertyBuilder::createProperty("Columns to Return")->isRequired(false)->withDescription(
    "A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. "
    "If no column names are supplied, all columns in the specified table will be returned. "
    "NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.")->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_maxValueColumnNames(
  core::PropertyBuilder::createProperty("Maximum-value Columns")->isRequired(false)->withDescription(
    "A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. "
    "Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. "
    "Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. "
    "This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. "
    "Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, and will result in error(s) during processing. "
    "If no columns are provided, all rows from the table will be considered, which could have a performance impact. "
    "NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly. "
    "NOTE: Because of a limitation of database access library 'soci', which doesn't support milliseconds in it's 'dt_date', "
    "there is a possibility that flowfiles might have duplicated records, if a max-value column with 'dt_date' type has value with milliseconds.")->
    supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_whereClause(
  core::PropertyBuilder::createProperty("db-fetch-where-clause")->isRequired(false)->withDescription(
    "A custom clause to be added in the WHERE condition when building SQL queries.")->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_sqlQuery(
  core::PropertyBuilder::createProperty("db-fetch-sql-query")->isRequired(false)->withDescription(
    "A custom SQL query used to retrieve data. Instead of building a SQL query from other properties, this query will be wrapped as a sub-query. "
    "Query must have no ORDER BY statement.")->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_maxRowsPerFlowFile(
  core::PropertyBuilder::createProperty("qdbt-max-rows")->isRequired(true)->withDefaultValue<int>(0)->withDescription(
    "The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. "
    "If the value specified is zero, then all rows are returned in a single FlowFile.")->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::s_stateDirectory(
  core::PropertyBuilder::createProperty("State Directory")->isRequired(false)->withDefaultValue("QDTState")->withDescription("DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.")->build());

const std::string QueryDatabaseTable::s_initialMaxValueDynamicPropertyPrefix("initial.maxvalue.");

const core::Relationship QueryDatabaseTable::s_success("success", "Successfully created FlowFile from SQL query result set.");

static const std::string ResultTableName = "tablename";
static const std::string ResultRowCount = "querydbtable.row.count";

static const std::string TABLENAME_KEY = "tablename";
static const std::string MAXVALUE_KEY_PREFIX = "maxvalue.";

// State
class LegacyState {
 public:
  LegacyState(const std::string& tableName, const std::string& stateDir, const std::string& uuid, std::shared_ptr<logging::Logger> logger)
    :tableName_(tableName), logger_(logger) {

    filePath_ = utils::file::FileUtils::concat_path(
            utils::file::FileUtils::concat_path(
              utils::file::FileUtils::concat_path(stateDir, "uuid"), uuid), "State.txt");

    if (!getStateFromFile())
      return;

    ok_ = true;
  }

  explicit operator bool() const {
    return ok_;
  }

  const std::unordered_map<std::string, std::string>& getStateMap() const {
    return mapState_;
  }

  bool moveStateFileToMigrated() {
    if (!ok_) {
      return false;
    }
    return rename(filePath_.c_str(), (filePath_ + "-migrated").c_str()) == 0;
  }

 private:
  static const std::string separator_;

   bool getStateFromFile() {
     std::string state;

     std::ifstream file(filePath_);
     if (!file) {
       return false;
     }

     std::stringstream ss;
     ss << file.rdbuf();

     state = ss.str();

     file.close();

     std::vector<std::string> listColumnNameValue;

     size_t pos = state.find(separator_, 0);
     if (pos == std::string::npos) {
       logger_->log_error("Invalid data in '%s' file.", filePath_.c_str());
       mapState_.clear();
       return false;
     }

     auto tableName = state.substr(0, pos);
     if (tableName != tableName_) {
       logger_->log_warn("tableName is changed - now: '%s', in State.txt: '%s'.", tableName_.c_str(), tableName.c_str());
       mapState_.clear();

       return false;
     }

     pos += separator_.size();

     while (true) {
       auto newPos = state.find(separator_, pos);
       if (newPos == std::string::npos)
         break;

       const std::string& columnNameValue = state.substr(pos, newPos - pos);
       listColumnNameValue.emplace_back(columnNameValue);

       pos = newPos + separator_.size();
     }

     for (const auto& columnNameValue : listColumnNameValue) {
       const auto posEQ = columnNameValue.find('=');
       if (posEQ == std::string::npos) {
         logger_->log_error("Invalid data in '%s' file.", filePath_.c_str());
         mapState_.clear();
         return false;
       }

       const auto& name = columnNameValue.substr(0, posEQ);
       const auto& value = columnNameValue.substr(posEQ + 1);

       mapState_.insert({ name, value });
     }

     return true;
   }

 private:
   std::unordered_map<std::string, std::string> mapState_;
   std::shared_ptr<logging::Logger> logger_;
   std::string filePath_;
   std::string tableName_;
   bool ok_{};
};

const std::string LegacyState::separator_ = "@!qdt!@";

// QueryDatabaseTable
QueryDatabaseTable::QueryDatabaseTable(const std::string& name, utils::Identifier uuid)
  : SQLProcessor(name, uuid) {
}

QueryDatabaseTable::~QueryDatabaseTable() {
}

void QueryDatabaseTable::initialize() {
  //! Set the supported properties
  setSupportedProperties( { dbControllerService(), outputFormat(), s_tableName, s_columnNames, s_maxValueColumnNames, s_whereClause, s_sqlQuery, s_maxRowsPerFlowFile, s_stateDirectory});

  //! Set the supported relationships
  setSupportedRelationships( { s_success });
}

void QueryDatabaseTable::processOnSchedule(core::ProcessContext &context) {
  initOutputFormat(context);

  context.getProperty(s_tableName.getName(), tableName_);
  context.getProperty(s_columnNames.getName(), columnNames_);

  context.getProperty(s_maxValueColumnNames.getName(), maxValueColumnNames_);
  listMaxValueColumnName_ = utils::inputStringToList(maxValueColumnNames_);

  context.getProperty(s_whereClause.getName(), whereClause_);
  context.getProperty(s_sqlQuery.getName(), sqlQuery_);
  context.getProperty(s_maxRowsPerFlowFile.getName(), maxRowsPerFlowFile_);

  mapState_.clear();

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map)) {
    if (state_map[TABLENAME_KEY] != tableName_) {
      state_manager_->clear();
    } else {
      for (auto&& elem : state_map) {
        if (elem.first.find(MAXVALUE_KEY_PREFIX) == 0) {
          mapState_.emplace(elem.first.substr(MAXVALUE_KEY_PREFIX.length()), std::move(elem.second));
        }
      }
    }
  } else {
    // Try to migrate legacy state file
    std::string stateDir;
    context.getProperty(s_stateDirectory.getName(), stateDir);
    if (!stateDir.empty()) {
      LegacyState legacyState(tableName_, stateDir, getUUIDStr(), logger_);
      if (legacyState) {
        mapState_ = legacyState.getStateMap();
        if (saveState() && state_manager_->persist()) {
          logger_->log_info("State migration successful");
          legacyState.moveStateFileToMigrated();
        } else {
          logger_->log_warn("Failed to persists migrated state");
        }
      } else {
        logger_->log_warn("Could not migrate state from specified State Directory %s", stateDir);
      }
    }
  }

  // If 'listMaxValueColumnName_' doesn't match columns in mapState_, then clear mapState_.
  if (listMaxValueColumnName_.size() != mapState_.size()) {
    mapState_.clear();
  } else {
    for (const auto& columName : listMaxValueColumnName_) {
      if (0 == mapState_.count(columName)) {
        mapState_.clear();
        break;
      }
    }
  }

  // Add in 'mapState_' new columns which are in 'listMaxValueColumnName_'.
  for (const auto& maxValueColumnName: listMaxValueColumnName_) {
    if (0 == mapState_.count(maxValueColumnName)) {
      mapState_.insert({maxValueColumnName, std::string()});
    }
  }

  const auto dynamic_prop_keys = context.getDynamicPropertyKeys();
  logger_->log_info("Received %zu dynamic properties", dynamic_prop_keys.size());

  // If the stored state for a max value column is empty, populate it with the corresponding initial max value, if it exists.
  for (const auto& key : dynamic_prop_keys) {
    if (std::string::npos == key.rfind(s_initialMaxValueDynamicPropertyPrefix, 0)) {
      throw minifi::Exception(PROCESSOR_EXCEPTION, "QueryDatabaseTable: Unsupported dynamic property \"" + key + "\"");
    }
    const auto columnName = utils::toLower(key.substr(s_initialMaxValueDynamicPropertyPrefix.length()));
    auto it = mapState_.find(columnName);
    if (it == mapState_.end()) {
      logger_->log_warn("Initial maximum value specified for column \"%s\", which is not specified as a Maximum-value Column. Ignoring.", columnName);
      continue;
    }
    if (!it->second.empty()) {
      continue;
    }
    std::string value;
    if (context.getDynamicProperty(key, value) && !value.empty()) {
      it->second = value;
      logger_->log_info("Setting initial maximum value of %s to %s", columnName, value);
    }
  }
}

void QueryDatabaseTable::processOnTrigger(core::ProcessSession& session) {
  const auto& selectQuery = getSelectQuery();

  logger_->log_info("QueryDatabaseTable: selectQuery: '%s'", selectQuery.c_str());

  auto statement = connection_->prepareStatement(selectQuery);

  auto rowset = statement->execute();

  int count = 0;
  size_t rowCount = 0;
  sql::MaxCollector maxCollector(selectQuery, maxValueColumnNames_, mapState_);
  sql::JSONSQLWriter sqlWriter(isJSONPretty());
  sql::SQLRowsetProcessor sqlRowsetProcessor(rowset, {&sqlWriter, &maxCollector});

  // Process rowset.
  do {
    rowCount = sqlRowsetProcessor.process(maxRowsPerFlowFile_ == 0 ? std::numeric_limits<size_t>::max() : maxRowsPerFlowFile_);
    count++;
    if (rowCount == 0)
      break;

    const auto output = sqlWriter.toString();
    if (!output.empty()) {
      WriteCallback writer(output);
      auto newflow = session.create();
      newflow->addAttribute(ResultRowCount, std::to_string(rowCount));
      newflow->addAttribute(ResultTableName, tableName_);
      session.write(newflow, &writer);
      session.transfer(newflow, s_success);
    }
  } while (rowCount > 0);

  const auto mapState = mapState_;
  if (maxCollector.updateMapState()) {
    try {
      session.commit();
    } catch (std::exception& e) {
      mapState_ = mapState;
      throw;
    }

    saveState();
  }
}

std::string QueryDatabaseTable::getSelectQuery() {
  std::string ret;

  if (sqlQuery_.empty()) {
    std::string columns;
    if (columnNames_.empty()) {
      columns = "*";
    } else {
      columns = columnNames_;
    }
    ret = "select " + columns + " from " + tableName_;
  } else {
    ret = sqlQuery_;
  }

  std::string whereClauses;

  if (!mapState_.empty() && !listMaxValueColumnName_.empty()) {
    for (auto index = 0U; index < listMaxValueColumnName_.size(); index++) {
      const auto& columnName = listMaxValueColumnName_[index];

      const auto itState = mapState_.find(columnName);

      const auto& maxValue = itState->second;
      if (maxValue.empty()) {
        continue;
      }

      // Logic to differentiate ">" vs ">=" based on index is copied from: 
      // https://github.com/apache/nifi/blob/master/nifi-nar-bundles/nifi-standard-bundle/nifi-standard-processors/src/main/java/org/apache/nifi/processors/standard/AbstractQueryDatabaseTable.java
      // (under comment "Add a condition for the WHERE clause"). And implementation explanation: https://issues.apache.org/jira/browse/NIFI-2712.
      if (index == 0) {
        whereClauses += columnName + " > ";
      } else {
        whereClauses += " and " + columnName + " >= ";
      }
      whereClauses += maxValue;
    }
  }

  if (!whereClause_.empty()) {
    whereClauses += " and " + whereClause_;
  }

  if (!whereClauses.empty()) {
    ret += " where " + whereClauses;
  }

  return ret;
}

bool QueryDatabaseTable::saveState() {
  std::unordered_map<std::string, std::string> state_map;
  state_map.emplace(TABLENAME_KEY, tableName_);
  for (const auto& elem : mapState_) {
    state_map.emplace(MAXVALUE_KEY_PREFIX + elem.first, elem.second);
  }
  return state_manager_->set(state_map);
}


} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
