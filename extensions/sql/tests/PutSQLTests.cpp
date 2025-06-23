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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "SQLTestController.h"

#include "processors/PutSQL.h"
#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"

TEST_CASE("Test Creation of PutSQL", "[PutSQLCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor>
      processor = std::make_shared<org::apache::nifi::minifi::processors::PutSQL>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Statement from processor property") {
  SQLTestController testController;

  auto plan = testController.createSQLPlan("PutSQL", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();

  auto input_file = plan->addInput({
    {"sql.args.1.value", "42"},
  });

  REQUIRE(sql_proc->setProperty(
      "SQL Statement",
      "INSERT INTO test_table (int_col, text_col) VALUES (?, 'asdf')"));

  plan->run();

  auto output = plan->getOutputs({"success", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);

  // Verify output state
  auto rows = testController.fetchValues();
  REQUIRE(rows.size() == 1);
  REQUIRE(rows[0].int_col == 42);
  REQUIRE(rows[0].text_col == "asdf");
}

TEST_CASE("Statement from flow file content") {
  SQLTestController testController;

  auto plan = testController.createSQLPlan("PutSQL", {{"success", "d"}});
  auto input_file = plan->addInput({
    {"sql.args.1.value", "4242"},
    {"sql.args.2.value", "fdsa"}
  }, "INSERT INTO test_table VALUES(?, ?);");

  plan->run();

  auto output = plan->getOutputs({"success", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);

  // Verify output state
  auto rows = testController.fetchValues();
  REQUIRE(rows.size() == 1);
  REQUIRE(rows[0].int_col == 4242);
  REQUIRE(rows[0].text_col == "fdsa");
}

TEST_CASE("PutSQL routes to failure on malformed statement") {
  SQLTestController testController;

  auto plan = testController.createSQLPlan("PutSQL", {{"success", "d"}, {"failure", "d"}});
  auto sql_proc = plan->getSQLProcessor();

  std::shared_ptr<core::FlowFile> input_file;
  SECTION("Missing parameter") {
    input_file = plan->addInput();
  }
// TODO(MINIFICPP-2002):
//  SECTION("Invalid parameter type") {
//    input_file = plan->addInput({
//      {"sql.args.1.value", "banana"},
//    });
//  }

  REQUIRE(sql_proc->setProperty(
      "SQL Statement",
      "INSERT INTO test_table (int_col, text_col) VALUES (?, 'asdf')"));

  plan->run();

  REQUIRE(plan->getOutputs({"success", "d"}).empty());
  auto output = plan->getOutputs({"failure", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);
}

TEST_CASE("PutSQL routes to failure on malformed content statement") {
  SQLTestController testController;

  auto plan = testController.createSQLPlan("PutSQL", {{"success", "d"}, {"failure", "d"}});

  std::shared_ptr<core::FlowFile> input_file;
  SECTION("No parameters") {
    input_file = plan->addInput({}, "INSERT INTO test_table VALUES(?, ?);");
  }
  SECTION("Not enough parameters") {
    input_file = plan->addInput({
      {"sql.args.1.value", "42"}
    }, "INSERT INTO test_table VALUES(?, ?);");
  }
// TODO(MINIFICPP-2001):
//  SECTION("Too many parameters") {
//    input_file = plan->addInput({
//      {"sql.args.1.value", "42"},
//      {"sql.args.2.value", "banana"},
//      {"sql.args.3.value", "too_many"}
//    }, "INSERT INTO test_table VALUES(?, ?);");
//  }
// TODO(MINIFICPP-2002):
//  SECTION("Invalid parameter type") {
//    input_file = plan->addInput({
//      {"sql.args.1.value", "banana"},
//      {"sql.args.2.value", "apple"}
//    }, "INSERT INTO test_table VALUES(?, ?);");
//  }
  SECTION("No such table") {
    input_file = plan->addInput({
      {"sql.args.1.value", "42"}
    }, "INSERT INTO no_such_table VALUES(?);");
  }
  SECTION("Garbage statement") {
    input_file = plan->addInput({
      {"sql.args.1.value", "42"}
    }, "ajshdjhasgdkashdiahfbuauwlkdkj;");
  }

  plan->run();

  REQUIRE(plan->getOutputs({"success", "d"}).empty());
  auto output = plan->getOutputs({"failure", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);
}
