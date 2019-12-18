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
#include <stdio.h>
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

static const core::Property s_dbControllerService(
    core::PropertyBuilder::createProperty("DB Controller Service")->isRequired(true)->withDescription("Database Controller Service.")->supportsExpressionLanguage(true)->build());

static const core::Property s_tableName(
  core::PropertyBuilder::createProperty("Table Name")->isRequired(true)->withDescription("The name of the database table to be queried.")->supportsExpressionLanguage(true)->build());

static const core::Property s_columnNames(
  core::PropertyBuilder::createProperty("Columns to Return")->isRequired(false)->withDescription(
    "A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. "
    "If no column names are supplied, all columns in the specified table will be returned. "
    "NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.")->supportsExpressionLanguage(true)->build());

static const core::Property s_maxValueColumnNames(
  core::PropertyBuilder::createProperty("Maximum-value Columns")->isRequired(false)->withDescription(
    "A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. "
    "Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. "
    "Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. "
    "This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. "
    "Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, and will result in error(s) during processing. "
    "If no columns are provided, all rows from the table will be considered, which could have a performance impact. "
    "NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly.")->supportsExpressionLanguage(true)->build());

static const core::Property s_whereClause(
  core::PropertyBuilder::createProperty("db-fetch-where-clause")->isRequired(false)->withDescription(
    "A custom clause to be added in the WHERE condition when building SQL queries.")->supportsExpressionLanguage(true)->build());

static const core::Property s_sqlQuery(
  core::PropertyBuilder::createProperty("db-fetch-sql-query")->isRequired(false)->withDescription(
    "A custom SQL query used to retrieve data. Instead of building a SQL query from other properties, this query will be wrapped as a sub-query. "
    "Query must have no ORDER BY statement.")->supportsExpressionLanguage(true)->build());

static const core::Property s_maxRowsPerFlowFile(
  core::PropertyBuilder::createProperty("qdbt-max-rows")->isRequired(true)->withDefaultValue<int>(0)->withDescription(
    "The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. "
    "If the value specified is zero, then all rows are returned in a single FlowFile.")->supportsExpressionLanguage(true)->build());

static core::Property s_stateDirectory(
  core::PropertyBuilder::createProperty("State Directory")->isRequired(false)->withDefaultValue("QDTState")->withDescription("Directory which contains processor state data.")->build());

static const core::Relationship s_success("success", "Successfully created FlowFile from SQL query result set.");

// Probably need to have `FragmentAttributes` like nifi has https://github.com/apache/nifi/blob/master/nifi-commons/nifi-utils/src/main/java/org/apache/nifi/flowfile/attributes/FragmentAttributes.java.
static const std::string FragmentId = "fragment.identifier";
static const std::string FragmentIndex = "fragment.index";
static const std::string FragmentCount = "fragment.count";

// State
class State {
 public:
  State(const std::string& stateDir, const std::string& uuid, std::shared_ptr<logging::Logger> logger)
    :logger_(logger) {
    if (!createUUIDDir(stateDir, uuid, filePath_))
      return;

    filePath_ += "State.txt";

    if (!getStateFromFile())
      return;

    ok_ = true;
  }

  ~State() {
    if (file_.is_open()) {
      file_.close();
    }
  }

  operator bool() const {
    return ok_;
  }

  std::unordered_map<std::string, std::string> mapState() const {
    return mapState_;
  }

  void writeStateToFile(const std::unordered_map<std::string, std::string>& mapState) {
    file_.seekp(std::ios::beg);

    auto dataSize = 0;
    for (const auto& el : mapState) {
      file_ << el.first << '=' << el.second << separator();
      dataSize += el.first.size() + 1 + el.second.size() + separator().size();
    }

    // Clear old data with ' '. 
    for (auto i = dataSize_ - dataSize; i > 0; i--) {
      file_ << ' ';
    }
    dataSize_ = dataSize;

    file_.flush();

    mapState_ = mapState;
  }

 private:
   static const std::string& separator() {
     static const std::string s_separator = "@!qdt!@";
     return s_separator;
   }

   bool createUUIDDir(const std::string& stateDir, const std::string& uuid, std::string& dir)
   {
     if (stateDir.empty()) {
       dir.clear();
       return false;
     }

     const auto dirSeparator = utils::file::FileUtils::get_separator();

     auto dirWithSlash = stateDir;
     if (stateDir.back() != dirSeparator) {
       dirWithSlash += dirSeparator;
     }

     dir = dirWithSlash + "uuid" + dirSeparator + uuid + dirSeparator;

     utils::file::FileUtils::create_dir(dir);

     auto dirCreated = utils::file::FileUtils::is_directory(dir.c_str());
     if (!dirCreated) {
       logger_->log_error("Cannot create %s", dir.c_str());
       dir.clear();
     }

     return dirCreated;
   }

   bool getStateFromFile() {
     std::string state;

     std::ifstream file(filePath_);
     if (!file) {
       return createEmptyStateFile();
     }

     std::stringstream ss;
     ss << file.rdbuf();

     state = ss.str();

     dataSize_ = state.size();

     file.close();

     std::vector<std::string> listColumnNameValue;

     size_t pos = 0;
     while (true) {
       auto newPos = state.find(separator(), pos);
       if (newPos == std::string::npos)
         break;

       const std::string& columnNameValue = state.substr(pos, newPos - pos);
       listColumnNameValue.emplace_back(columnNameValue);

       pos = newPos + separator().size();
     }

     for (const auto& columnNameValue : listColumnNameValue) {
       const auto posEQ = columnNameValue.find('=');
       if (posEQ == std::string::npos) {
         logger_->log_error("Invalid data in '%s' file.", filePath_.c_str());
         mapState_.clear();
         return createEmptyStateFile();
       }

       const auto& name = columnNameValue.substr(0, posEQ);
       const auto& value = columnNameValue.substr(posEQ + 1);

       mapState_.insert({ name, value });
     }

     file_.open(filePath_);
     if (!file_.is_open()) {
       logger_->log_error("Cannot open %s", filePath_.c_str());
       mapState_.clear();
       return false;
     }

     return true;
   }

   bool createEmptyStateFile() {
     file_.open(filePath_, std::ios::out);
     if (!file_.is_open()) {
       logger_->log_error("Cannot open '%s' file", filePath_.c_str());
       return false;
     }

     return true;
   }

 private:
   std::unordered_map<std::string, std::string> mapState_;
   std::shared_ptr<logging::Logger> logger_;
   std::string filePath_;
   std::fstream file_;
   int dataSize_{};
   bool ok_{};
};


// QueryDatabaseTable
QueryDatabaseTable::QueryDatabaseTable(const std::string& name, utils::Identifier uuid)
  : core::Processor(name, uuid), logger_(logging::LoggerFactory<QueryDatabaseTable>::getLogger()) {
}

QueryDatabaseTable::~QueryDatabaseTable() {
}

void QueryDatabaseTable::initialize() {
  //! Set the supported properties
  setSupportedProperties( { s_dbControllerService, s_tableName, s_columnNames, s_maxValueColumnNames, s_whereClause, s_sqlQuery, s_maxRowsPerFlowFile, s_stateDirectory });

  //! Set the supported relationships
  setSupportedRelationships( { s_success });
}

void QueryDatabaseTable::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(s_dbControllerService.getName(), db_controller_service_);
  context->getProperty(s_tableName.getName(), tableName_);

  context->getProperty(s_columnNames.getName(), columnNames_);
  //  parseCommaSeparatedString(columnNames, listColumnNames_);

  context->getProperty(s_maxValueColumnNames.getName(), maxValueColumnNames_);
  listMaxValueColumnName_ = utils::inputStringToList(maxValueColumnNames_);

  context->getProperty(s_whereClause.getName(), whereClause_);
  context->getProperty(s_sqlQuery.getName(), sqlQuery_);
  context->getProperty(s_maxRowsPerFlowFile.getName(), maxRowsPerFlowFile_);

  std::string stateDir;
  context->getProperty(s_stateDirectory.getName(), stateDir);
  if (stateDir.empty()) {
    logger_->log_error("State Directory is empty");
    return;
  }

  pState_ = std::make_unique<State>(stateDir, getUUIDStr(), logger_);
  if (!*pState_) {
    return;
  }
  
  mapState_ = pState_->mapState();

  // Remove from 'mapState_' columns which are not in 'listMaxValueColumnName_'.
  for (auto itMapState = mapState_.begin(); itMapState != mapState_.end(); ) {
    if (listMaxValueColumnName_.end() == std::find(listMaxValueColumnName_.begin(), listMaxValueColumnName_.end(), itMapState->first)) {
      itMapState = mapState_.erase(itMapState);
    } else {
      ++itMapState;
    }
  }

  // Add in 'mapState_' new columns which are in 'listMaxValueColumnName_'.
  for (const auto& maxValueColumnName: listMaxValueColumnName_) {
    if (0 == mapState_.count(maxValueColumnName)) {
      mapState_.insert({maxValueColumnName, std::string()});
    }
  }

  database_service_ = std::dynamic_pointer_cast<sql::controllers::DatabaseService>(context->getControllerService(db_controller_service_));
  if (!database_service_) 
    throw minifi::Exception(PROCESSOR_EXCEPTION, "'DB Controller Service' must be defined");
}

void QueryDatabaseTable::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("'onTrigger' is called before previous 'onTrigger' call is finished.");
    context->yield();
    return;
  }

  if (!connection_) {
    connection_ = database_service_->getConnection();
    if (!connection_) {
      context->yield();
      return;
    }
  }

  try {
    const auto& selectQuery = getSelectQuery();

    auto statement = connection_->prepareStatement(selectQuery);

    auto rowset = statement->execute();

    int count = 0;
    size_t row_count = 0;
    std::stringstream outputStream;
    sql::MaxCollector maxCollector(selectQuery, maxValueColumnNames_, mapState_);
    sql::JSONSQLWriter writer(rowset, &outputStream, &maxCollector);
    // serialize the rows
    do {
      row_count = writer.serialize(maxRowsPerFlowFile_ == 0 ? std::numeric_limits<size_t>::max() : maxRowsPerFlowFile_);
      count++;
      if (row_count == 0)
        break;
      writer.write();

      auto output = outputStream.str();
      if (!output.empty()) {
        WriteCallback writer(output.data(), output.size());
        auto newflow = session->create();
        newflow->addAttribute("executesql.resultset.index", std::to_string(row_count));
        session->write(newflow, &writer);
        session->transfer(newflow, s_success);
      }

      outputStream.str("");
      outputStream.clear();
    } while (row_count > 0);

    if (maxCollector.updateMapState()) {
      pState_->writeStateToFile(mapState_);
    }
  }
  catch (std::exception& e) {
    logger_->log_error(e.what());
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

void QueryDatabaseTable::notifyStop() {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
