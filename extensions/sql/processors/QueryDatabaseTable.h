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

#include "core/Resource.h"
#include "core/ProcessSession.h"
#include "SQLProcessor.h"
#include "FlowFileSource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! QueryDatabaseTable Class
class QueryDatabaseTable: public SQLProcessor, public FlowFileSource {
 public:
  explicit QueryDatabaseTable(const std::string& name, utils::Identifier uuid = utils::Identifier());

  static const std::string RESULT_TABLE_NAME;
  static const std::string RESULT_ROW_COUNT;

  static const std::string TABLENAME_KEY;
  static const std::string MAXVALUE_KEY_PREFIX;

  //! Processor Name
  static const std::string ProcessorName;

  static const core::Property TableName;
  static const core::Property ColumnNames;
  static const core::Property MaxValueColumnNames;
  static const core::Property WhereClause;

  static const std::string InitialMaxValueDynamicPropertyPrefix;

  static const core::Relationship Success;

  bool supportsDynamicProperties() override {
    return true;
  }

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;

 private:
  std::string buildSelectQuery();

  void initializeMaxValues(core::ProcessContext& context);
  void loadMaxValuesFromDynamicProperties(core::ProcessContext& context);

  bool saveState();

 private:
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::string table_name_;
  std::string queried_columns_;
  std::string extra_where_clause_;
  std::vector<std::string> max_value_columns_;
  std::unordered_map<std::string, std::string> max_values_;
};

REGISTER_RESOURCE(QueryDatabaseTable, "QueryDatabaseTable to execute SELECT statement via ODBC.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
