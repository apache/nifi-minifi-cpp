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

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "services/DatabaseService.h"
#include "SQLProcessor.h"
#include "OutputFormat.h"

#include <sstream>
#include <unordered_map>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class State;

//! QueryDatabaseTable Class
class QueryDatabaseTable: public SQLProcessor<QueryDatabaseTable>, public OutputFormat {
 public:
  explicit QueryDatabaseTable(const std::string& name, utils::Identifier uuid = utils::Identifier());
  virtual ~QueryDatabaseTable();

  //! Processor Name
  static const std::string ProcessorName;

  static const core::Property s_tableName;
  static const core::Property s_columnNames;
  static const core::Property s_maxValueColumnNames;
  static const core::Property s_whereClause;
  static const core::Property s_sqlQuery;
  static const core::Property s_maxRowsPerFlowFile;
  static const core::Property s_stateDirectory;

  static const std::string s_initialMaxValueDynamicPropertyPrefix;

  static const core::Relationship s_success;

  bool supportsDynamicProperties() override {
    return true;
  }

  void processOnSchedule(core::ProcessContext& context);
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session);
  
  void initialize() override;

 private:
  std::string getSelectQuery();

  bool saveState();

 private:
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::string tableName_;
  std::string columnNames_;
  std::string maxValueColumnNames_;
  std::string whereClause_;
  std::string sqlQuery_;
  int maxRowsPerFlowFile_{};
  std::vector<std::string> listMaxValueColumnName_;
  std::unordered_map<std::string, std::string> mapState_;
  std::unordered_map<std::string, soci::data_type> mapColumnType_;
};

REGISTER_RESOURCE(QueryDatabaseTable, "QueryDatabaseTable to execute SELECT statement via ODBC.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
