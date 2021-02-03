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

#undef NDEBUG

#include "../TestBase.h"
#include "SQLTestController.h"

#include "processors/PutSQL.h"
#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "SQLiteConnection.h"

TEST_CASE("Test Creation of PutSQL", "[PutSQLCreate]") {  // NOLINT
  TestController testController;
  std::shared_ptr<core::Processor>
      processor = std::make_shared<org::apache::nifi::minifi::processors::PutSQL>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Put", "[PutSQLPut]") {  // NOLINT
  SQLTestController testController;

  auto plan = testController.createPlan();
  testController.initSQLService(plan);

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
  auto put = plan->addProcessor(
      "PutSQL",
      "PutSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put,
      "DB Controller Service",
      "ODBCService");
  plan->setProperty(
      put,
      "SQL Statement",
      "INSERT INTO test_table (int_col, text_col) VALUES (?, ?)");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // PutSQL

  // Verify output state
  auto rows = testController.fetchValues();
  REQUIRE(rows.size() == 1);
  REQUIRE(rows[0].int_col == 42);
  REQUIRE(rows[0].text_col == "asdf");
}

TEST_CASE("Test Put Content", "[PutSQLPutContent]") {  // NOLINT
  SQLTestController testController;

  auto plan = testController.createPlan();
  testController.initSQLService(plan);

  // Define directory for test input file
  utils::Path test_in_dir = testController.createTempDir("/var/tmp/gt.XXXXXX");

  // Define test input file
  utils::Path test_file = test_in_dir / "test.in";

  // Write test SQL content
  {
    std::ofstream os(test_file.str());
    os << "INSERT INTO test_table VALUES(?, ?);";
  }

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), test_in_dir.str());
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
  auto put = plan->addProcessor(
      "PutSQL",
      "PutSQL",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put,
      "DB Controller Service",
      "ODBCService");

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // PutSQL

  // Verify output state
  auto rows = testController.fetchValues();
  REQUIRE(rows.size() == 1);
  REQUIRE(rows[0].int_col == 4242);
  REQUIRE(rows[0].text_col == "fdsa");
}

