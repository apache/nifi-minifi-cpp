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
#include "Utils.h"
#include "FlowFileMatcher.h"

TEST_CASE("QueryDatabaseTable queries the table and returns specified columns", "[QueryDatabaseTable1]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"}
  });

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);

  std::string row_count;
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
    [{"text_col": "one"}, {"text_col": "two"}, {"text_col": "three"}]
  )");
}

TEST_CASE("QueryDatabaseTable requerying the table returns only new rows", "[QueryDatabaseTable2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"}
  });

  plan->run();

  auto first_flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(first_flow_files.size() == 1);

  controller.insertValues({
    {104, "four"},
    {105, "five"}
  });

  SECTION("Without schedule") {plan->run();}
  SECTION("With schedule") {plan->run(true);}

  auto second_flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(second_flow_files.size() == 1);

  std::string row_count;
  second_flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");
  auto content = plan->getContent(second_flow_files[0]);
  verifyJSON(content, R"(
    [{"text_col": "four"}, {"text_col": "five"}]
  )");
}

TEST_CASE("QueryDatabaseTable specifying initial max values", "[QueryDatabaseTable2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");
  sql_proc->setDynamicProperty("initial.maxvalue.int_col", "102");

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"},
    {104, "four"}
  });

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);

  std::string row_count;
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");
  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
    [{"text_col": "three"}, {"text_col": "four"}]
  )");
}

TEST_CASE("QueryDatabaseTable honors Max Rows Per Flow File and sets output attributes", "[QueryDatabaseTable1]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxRowsPerFlowFile.getName(), "3");

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"},
    {104, "four"},
    {105, "five"}
  });

  plan->run();

  auto content_verifier = [&] (const std::shared_ptr<core::FlowFile>& actual, const std::string& expected) {
    verifyJSON(plan->getContent(actual), expected);
  };

  FlowFileMatcher matcher(content_verifier, {
      processors::QueryDatabaseTable::RESULT_TABLE_NAME,
      processors::QueryDatabaseTable::RESULT_ROW_COUNT,
      processors::QueryDatabaseTable::FRAGMENT_COUNT,
      processors::QueryDatabaseTable::FRAGMENT_INDEX,
      processors::QueryDatabaseTable::FRAGMENT_IDENTIFIER,
      "maxvalue.int_col"
  });

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 2);

  matcher.verify(flow_files[0],
    {"test_table", "3", "2", "0", var("frag_id"), "105"},
    R"([{"text_col": "one"}, {"text_col": "two"}, {"text_col": "three"}])");
  matcher.verify(flow_files[1],
    {"test_table", "2", "2", "1", var("frag_id"), "105"},
    R"([{"text_col": "four"}, {"text_col": "five"}])");
}

TEST_CASE("QueryDatabaseTable changing table name resets state", "[QueryDatabaseTable2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");

  controller.insertValues({
      {101, "one"},
      {102, "two"},
      {103, "three"}
  });

  // query "test_table"
  plan->run();
  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");


  // query "empty_test_table"
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "empty_test_table");
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 0);

  // again query "test_table", by now the stored state is reset, so all rows are returned
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
}

TEST_CASE("QueryDatabaseTable changing maximum value columns resets state", "[QueryDatabaseTable2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(processors::QueryDatabaseTable::TableName.getName(), "test_table");
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  sql_proc->setProperty(processors::QueryDatabaseTable::ColumnNames.getName(), "text_col");

  controller.insertValues({
      {101, "one"},
      {102, "two"},
      {103, "three"}
  });

  // query using ["int_col"] as max value columns
  plan->run();
  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");


  // query using ["int_col", "text_col"] as max value columns
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col, text_col");
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");

  // query using ["int_col"] as max value columns again
  sql_proc->setProperty(processors::QueryDatabaseTable::MaxValueColumnNames.getName(), "int_col");
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
}
