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

#include <uuid/uuid.h>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <iostream>
#include <GenerateFlowFile.h>
#include <UpdateAttribute.h>
#include <LogAttribute.h>
#include <ExecuteSQL.h>

#include "../TestBase.h"

#include "processors/GetFile.h"
#include "processors/PutFile.h"

#include "PutSQL.h"

TEST_CASE("Test Creation of PutSQL", "[PutSQLCreate]") {  // NOLINT
  TestController testController;
  std::shared_ptr<core::Processor>
      processor = std::make_shared<org::apache::nifi::minifi::processors::PutSQL>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Put", "[PutSQLPut]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::PutSQL>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test db
  std::string test_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_dir[0]) != nullptr);

  // Define test db file
  std::string test_db(test_dir);
  test_db.append("/test.db");

  // Create test db
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);");
    stmt.step();
    REQUIRE(stmt.is_ok());
  }

  // Build MiNiFi processing graph
  auto generate = plan->addProcessor(
      "GenerateFlowFile",
      "Generate");
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "Update",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      update,
      "sql.args.1.value",
      "42",
      true);
  plan->setProperty(
      update,
      "sql.args.2.value",
      "asdf",
      true);
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);
  auto put = plan->addProcessor(
      "PutSQL",
      "PutSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put,
      "Connection URL",
      "sqlite://" + test_db);
  plan->setProperty(
      put,
      "SQL Statement",
      "INSERT INTO test_table (int_col, text_col) VALUES (?, ?)");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutSQL

  // Verify output state
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("SELECT int_col, text_col FROM test_table;");
    stmt.step();
    REQUIRE(stmt.is_ok());
    REQUIRE(42 == stmt.column_int64(0));
    REQUIRE("asdf" == stmt.column_text(1));
  }
}

TEST_CASE("Test Put Content", "[PutSQLPutContent]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::PutSQL>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test db
  std::string test_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_dir[0]) != nullptr);

  // Define test db file
  std::string test_db(test_dir);
  test_db.append("/test.db");

  // Define directory for test input file
  std::string test_in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_in_dir[0]) != nullptr);

  // Define test input file
  std::string test_file(test_in_dir);
  test_file.append("/test.in");

  // Write test SQL content
  {
    std::ofstream os(test_file);
    os << "INSERT INTO test_table VALUES(?, ?);";
  }

  // Create test db
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);");
    stmt.step();
    REQUIRE(stmt.is_ok());
  }

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), test_in_dir);
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "Update",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      update,
      "sql.args.1.value",
      "4242",
      true);
  plan->setProperty(
      update,
      "sql.args.2.value",
      "fdsa",
      true);
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);
  auto put = plan->addProcessor(
      "PutSQL",
      "PutSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put,
      "Connection URL",
      "sqlite://" + test_db);

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutSQL

  // Verify output state
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("SELECT int_col, text_col FROM test_table;");
    stmt.step();
    REQUIRE(stmt.is_ok());
    REQUIRE(4242 == stmt.column_int64(0));
    REQUIRE("fdsa" == stmt.column_text(1));
  }
}

TEST_CASE("Test Exec", "[ExecuteSQL]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::ExecuteSQL>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test db
  std::string test_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_dir[0]) != nullptr);

  // Define test db file
  std::string test_db(test_dir);
  test_db.append("/test.db");

  // Create test db
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("CREATE TABLE test_table (int_col INTEGER, text_col TEXT);");
    stmt.step();
    REQUIRE(stmt.is_ok());

    // Insert test data
    auto stmt2 = db.prepare("INSERT INTO test_table (int_col, text_col) VALUES (42, 'asdf');");
    stmt2.step();
    REQUIRE(stmt2.is_ok());
  }

  // Build MiNiFi processing graph
  auto exec = plan->addProcessor(
      "ExecuteSQL",
      "ExecuteSQL");
  plan->setProperty(
      exec,
      "Connection URL",
      "sqlite://" + test_db);
  plan->setProperty(
      exec,
      "SQL Statement",
      "SELECT * FROM test_table;");
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);

  plan->runNextProcessor();  // Exec
  plan->runNextProcessor();  // Log

  // Verify output state
  REQUIRE(LogTestController::getInstance().contains("key:int_col value:42"));
  REQUIRE(LogTestController::getInstance().contains("key:text_col value:asdf"));
}

TEST_CASE("Test Exec 2", "[ExecuteSQL2]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
  LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
  LogTestController::getInstance().setTrace<processors::ExecuteSQL>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test db
  std::string test_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_dir[0]) != nullptr);

  // Define test db file
  std::string test_db(test_dir);
  test_db.append("/test.db");

  // Create test db
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("CREATE TABLE test_table (id_col INTEGER, int_col INTEGER, text_col TEXT);");
    stmt.step();
    REQUIRE(stmt.is_ok());

    // Insert test data
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (1, 33, 'aaaa');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (2, 42, 'bbbb');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (3, 24, 'cccc');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
  }

  // Build MiNiFi processing graph
  auto generate = plan->addProcessor(
      "GenerateFlowFile",
      "Generate");
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "Update",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      update,
      "sql.args.1.value",
      "2",
      true);
  auto exec = plan->addProcessor(
      "ExecuteSQL",
      "ExecuteSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      exec,
      "Connection URL",
      "sqlite://" + test_db);
  plan->setProperty(
      exec,
      "SQL Statement",
      "SELECT * FROM test_table WHERE id_col = ?;");
  std::set<core::Relationship> auto_term_rels;
  core::Relationship original("original", "");
  auto_term_rels.insert(original);
  exec->setAutoTerminatedRelationships(auto_term_rels);
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);

  plan->runNextProcessor();  // Gen
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Exec
  plan->runNextProcessor();  // Log

  // Verify output state
  REQUIRE(LogTestController::getInstance().contains("key:id_col value:2"));
  REQUIRE(LogTestController::getInstance().contains("key:int_col value:42"));
  REQUIRE(LogTestController::getInstance().contains("key:text_col value:bbbb"));
}

TEST_CASE("Test Exec 3", "[ExecuteSQL3]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::ExecuteSQL>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test db
  std::string test_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_dir[0]) != nullptr);

  // Define test db file
  std::string test_db(test_dir);
  test_db.append("/test.db");

  // Define directory for test input file
  std::string test_in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_in_dir[0]) != nullptr);

  // Define test input file
  std::string test_file(test_in_dir);
  test_file.append("/test.in");

  // Write test SQL content
  {
    std::ofstream os(test_file);
    os << "SELECT text_col FROM test_table WHERE id_col = ?;";
  }

  // Create test db
  {
    minifi::sqlite::SQLiteConnection db(test_db);
    auto stmt = db.prepare("CREATE TABLE test_table (id_col INTEGER, int_col INTEGER, text_col TEXT);");
    stmt.step();
    REQUIRE(stmt.is_ok());

    // Insert test data
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (1, 33, 'aaaa');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (2, 42, 'bbbb');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
    {
      auto ins = db.prepare("INSERT INTO test_table (id_col, int_col, text_col) VALUES (3, 24, 'cccc');");
      ins.step();
      REQUIRE(ins.is_ok());
    }
  }

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), test_in_dir);
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "Update",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      update,
      "sql.args.1.value",
      "2",
      true);
  auto exec = plan->addProcessor(
      "ExecuteSQL",
      "ExecuteSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      exec,
      "Connection URL",
      "sqlite://" + test_db);
  std::set<core::Relationship> auto_term_rels;
  core::Relationship original("original", "");
  auto_term_rels.insert(original);
  exec->setAutoTerminatedRelationships(auto_term_rels);
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Exec
  plan->runNextProcessor();  // Log

  // Verify output state
  REQUIRE(LogTestController::getInstance().contains("key:text_col value:bbbb"));
}
