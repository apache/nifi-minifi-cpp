/**
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

#include "../TestBase.h"

#include "processors/PutSQL.h"
#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/ExecuteSQL.h"
#include "processors/QueryDatabaseTable.h"
#include "SQLTestPlan.h"

#include "services/ODBCConnector.h"

struct TableRow {
  int64_t int_col;
  std::string text_col;
};

#ifdef WIN32
const std::string DRIVER = "{SQLite3 ODBC Driver}";
#else
const std::string DRIVER = "libsqlite3odbc.so";
#endif

class SQLTestController : public TestController {
 public:
  SQLTestController() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
    LogTestController::getInstance().setTrace<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<processors::PutSQL>();
    LogTestController::getInstance().setTrace<processors::ExecuteSQL>();
    LogTestController::getInstance().setTrace<processors::QueryDatabaseTable>();

    char format[] = "/var/tmp/gt.XXXXXX";
    test_dir_ = createTempDirectory(format);
    database_ = test_dir_ / "test.db";
    connection_str_ = "Driver=" + DRIVER + ";Database=" + database_.str();

    // Create test dbs
    minifi::sql::controllers::ODBCConnection{connection_str_}.prepareStatement("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);")->execute();
    minifi::sql::controllers::ODBCConnection{connection_str_}.prepareStatement("CREATE TABLE empty_test_table (int_col INTEGER, text_col TEXT);")->execute();
  }

  std::shared_ptr<SQLTestPlan> createSQLPlan(const std::string& sql_processor, std::initializer_list<core::Relationship> outputs) {
    return std::make_shared<SQLTestPlan>(*this, connection_str_, sql_processor, outputs);
  }

  void insertValues(std::initializer_list<TableRow> values) {
    minifi::sql::controllers::ODBCConnection connection{connection_str_};
    for (const auto& value : values) {
      connection.prepareStatement("INSERT INTO test_table (int_col, text_col) VALUES (?, ?);")
          ->execute({std::to_string(value.int_col), value.text_col});
    }
  }

  std::vector<TableRow> fetchValues() {
    std::vector<TableRow> rows;
    minifi::sql::controllers::ODBCConnection connection{connection_str_};
    auto soci_rowset = connection.prepareStatement("SELECT * FROM test_table;")->execute();
    for (const auto& soci_row : soci_rowset) {
      rows.push_back(TableRow{get_column_cast<int64_t>(soci_row, "int_col"), soci_row.get<std::string>("text_col")});
    }
    return rows;
  }

  utils::Path getDB() const {
    return database_;
  }

 private:
  template<typename T>
  T get_column_cast(const soci::row& row, const std::string& column_name) {
    const auto& column_props = row.get_properties(column_name);
    switch (const auto data_type = column_props.get_data_type()) {
      case soci::data_type::dt_integer:
        return gsl::narrow<T>(row.get<int>(column_name));
      case soci::data_type::dt_long_long:
        return gsl::narrow<T>(row.get<long long>(column_name));
      case soci::data_type::dt_unsigned_long_long:
        return gsl::narrow<T>(row.get<unsigned long long>(column_name));
      default:
        throw std::logic_error("Unknown data type for column \"" + column_name + "\"");
    }
  }

  utils::Path test_dir_;
  utils::Path database_;
  std::string connection_str_;
};
