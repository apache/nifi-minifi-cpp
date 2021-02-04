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

  plan->run();

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
