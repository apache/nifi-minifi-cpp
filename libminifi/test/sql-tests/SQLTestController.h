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

#include "SQLiteConnection.h"
#include "processors/PutSQL.h"
#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/ExecuteSQL.h"
#include "processors/QueryDatabaseTable.h"
#include "SQLTestPlan.h"

struct TableRow {
  int64_t int_col;
  std::string text_col;
};

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

    test_dir_ = createTempDir("/var/tmp/gt.XXXXXX");
    database_ = test_dir_ / "test.db";

    // Create test db
    {
      minifi::sqlite::SQLiteConnection db(database_.str());
      auto stmt = db.prepare("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);");
      stmt.step();
      REQUIRE(stmt.is_ok());
    }
  }

  std::shared_ptr<SQLTestPlan> createSQLPlan(const std::string& sql_processor, std::initializer_list<core::Relationship> outputs) {
    return std::make_shared<SQLTestPlan>(*this, database_, sql_processor, outputs);
  }

  void insertValues(std::initializer_list<TableRow> values) {
    minifi::sqlite::SQLiteConnection db(database_.str());
    for (const auto& value : values) {
      auto stmt = db.prepare("INSERT INTO test_table (int_col, text_col) VALUES (?, ?);");
      stmt.bind_int64(1, value.int_col);
      stmt.bind_text(2, value.text_col);
      stmt.step();
      REQUIRE(stmt.is_ok());
    }
  }

  std::vector<TableRow> fetchValues() {
    std::vector<TableRow> rows;
    minifi::sqlite::SQLiteConnection db(database_.str());
    auto stmt = db.prepare("SELECT * FROM test_table;");
    while (true) {
      stmt.step();
      REQUIRE(stmt.is_ok());
      if (stmt.is_done()) {
        break;
      }
      rows.push_back(TableRow{stmt.column_int64(0), stmt.column_text(1)});
    }
    return rows;
  }

  utils::Path getDB() const {
    return database_;
  }

 private:
  utils::Path test_dir_;
  utils::Path database_;
};
