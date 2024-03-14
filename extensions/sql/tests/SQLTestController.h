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

#include <memory>
#include <vector>
#include <string>

#include "TestBase.h"
#include "Catch.h"

#include "processors/PutSQL.h"
#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/ExecuteSQL.h"
#include "processors/QueryDatabaseTable.h"
#include "SQLTestPlan.h"

#ifdef MINIFI_USE_REAL_ODBC_TEST_DRIVER
#include "services/ODBCConnector.h"
using ODBCConnection = minifi::sql::ODBCConnection;
#else
#include "mocks/MockConnectors.h"
using ODBCConnection = minifi::sql::MockODBCConnection;
#endif

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
    LogTestController::getInstance().setTrace<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::processors::UpdateAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::PutSQL>();
    LogTestController::getInstance().setTrace<minifi::processors::ExecuteSQL>();
    LogTestController::getInstance().setTrace<minifi::processors::QueryDatabaseTable>();

    test_dir_ = createTempDirectory();
    database_ = test_dir_ / "test.db";
    connection_str_ = "Driver=" + DRIVER + ";Database=" + database_.string();

    // Create test dbs
    ODBCConnection{connection_str_}.prepareStatement("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);")->execute();
    ODBCConnection{connection_str_}.prepareStatement("CREATE TABLE empty_test_table (int_col INTEGER, text_col TEXT);")->execute();
  }

  std::shared_ptr<SQLTestPlan> createSQLPlan(const std::string& sql_processor, std::initializer_list<core::Relationship> outputs) {
    return std::make_shared<SQLTestPlan>(*this, connection_str_, sql_processor, outputs);
  }

  void insertValues(std::initializer_list<TableRow> values) {
    ODBCConnection connection{connection_str_};
    for (const auto& value : values) {
      connection.prepareStatement("INSERT INTO test_table (int_col, text_col) VALUES (?, ?);")
          ->execute({std::to_string(value.int_col), value.text_col});
    }
  }

  std::vector<TableRow> fetchValues() {
    std::vector<TableRow> rows;
    ODBCConnection connection{connection_str_};
    auto rowset = connection.prepareStatement("SELECT * FROM test_table;")->execute();
    for (rowset->reset(); !rowset->is_done(); rowset->next()) {
      const auto& row = rowset->getCurrent();
      rows.push_back(TableRow{row.getInteger(0), row.getString(1)});
    }
    return rows;
  }

  auto getDB() const {
    return database_;
  }

 private:
  std::filesystem::path test_dir_;
  std::filesystem::path database_;
  std::string connection_str_;
};
