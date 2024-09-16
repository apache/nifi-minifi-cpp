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
#include "unit/TestUtils.h"
#include "FlowFileMatcher.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("QueryDatabaseTable queries the table and returns specified columns", "[QueryDatabaseTable1]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"}
  });

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);

  std::string row_count;
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{"text_col": "one"}, {"text_col": "two"}, {"text_col": "three"}]
  )", true);
}

TEST_CASE("QueryDatabaseTable requerying the table returns only new rows", "[QueryDatabaseTable2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));

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

  SECTION("Run onTrigger only") {plan->run();}
  SECTION("Run both onSchedule and onTrigger") {plan->run(true);}

  auto second_flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(second_flow_files.size() == 1);

  std::string row_count;
  second_flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");
  auto content = plan->getContent(second_flow_files[0]);
  utils::verifyJSON(content, R"(
    [{"text_col": "four"}, {"text_col": "five"}]
  )", true);
}

TEST_CASE("QueryDatabaseTable specifying initial max values", "[QueryDatabaseTable3]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));
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
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");
  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{"text_col": "three"}, {"text_col": "four"}]
  )", true);
}

TEST_CASE("QueryDatabaseTable honors Max Rows Per Flow File and sets output attributes", "[QueryDatabaseTable4]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxRowsPerFlowFile.name, "3"));

  controller.insertValues({
    {101, "one"},
    {102, "two"},
    {103, "three"},
    {104, "four"},
    {105, "five"}
  });

  plan->run();

  auto content_verifier = [&] (const std::shared_ptr<core::FlowFile>& actual, const std::string& expected) {
    utils::verifyJSON(plan->getContent(actual), expected, true);
  };

  FlowFileMatcher matcher(content_verifier, {
      minifi::processors::QueryDatabaseTable::RESULT_TABLE_NAME,
      minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT,
      minifi::processors::QueryDatabaseTable::FRAGMENT_COUNT,
      minifi::processors::QueryDatabaseTable::FRAGMENT_INDEX,
      minifi::processors::QueryDatabaseTable::FRAGMENT_IDENTIFIER,
      "maxvalue.int_col"
  });

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 2);

  std::optional<std::string> fragment_id;

  matcher.verify(flow_files[0],
    { AttributeValue("test_table"), AttributeValue("3"), AttributeValue("2"), AttributeValue("0"), capture(fragment_id), AttributeValue("105") },
    R"([{"text_col": "one"}, {"text_col": "two"}, {"text_col": "three"}])");
  REQUIRE(fragment_id);
  matcher.verify(flow_files[1],
    { AttributeValue("test_table"), AttributeValue("2"), AttributeValue("2"), AttributeValue("1"), AttributeValue(*fragment_id), AttributeValue("105") },
    R"([{"text_col": "four"}, {"text_col": "five"}])");
}

TEST_CASE("QueryDatabaseTable changing table name resets state", "[QueryDatabaseTable5]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));

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
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");


  // query "empty_test_table"
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "empty_test_table"));
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.empty());

  // again query "test_table", by now the stored state is reset, so all rows are returned
  // set through ProcessContext to avoid corrupting ProcessContextExpr cache
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
}

TEST_CASE("QueryDatabaseTable changing maximum value columns resets state", "[QueryDatabaseTable6]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("QueryDatabaseTable", {{"success", "d"}});
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::TableName.name, "test_table"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::ColumnNames.name, "text_col"));

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
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");


  // query using ["int_col", "text_col"] as max value columns
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col, text_col"));
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");

  // query using ["int_col"] as max value columns again
  CHECK(plan->setProperty(minifi::processors::QueryDatabaseTable::MaxValueColumnNames.name, "int_col"));
  plan->run(true);
  flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  flow_files[0]->getAttribute(minifi::processors::QueryDatabaseTable::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "3");
}

}  // namespace org::apache::nifi::minifi::test
