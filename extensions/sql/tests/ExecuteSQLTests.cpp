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
#include <optional>

#include "SQLTestController.h"
#include "processors/ExecuteSQL.h"
#include "unit/TestUtils.h"
#include "FlowFileMatcher.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("ExecuteSQL works without incoming flow file", "[ExecuteSQL1]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty("SQL select query", "SELECT * FROM test_table ORDER BY int_col ASC");

  controller.insertValues({{11, "one"}, {22, "two"}});

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(minifi::processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");

  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{
      "int_col": 11,
      "text_col": "one"
    },{
      "int_col": 22,
      "text_col": "two"
    }]
  )");
}

TEST_CASE("ExecuteSQL uses statement in property", "[ExecuteSQL2]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty("SQL select query", "SELECT * FROM test_table WHERE int_col = ${int_col_value}");

  controller.insertValues({{11, "one"}, {22, "two"}});

  auto input_file = plan->addInput({{"int_col_value", "11"}});

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(minifi::processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "1");

  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{
      "int_col": 11,
      "text_col": "one"
    }]
  )");
}

TEST_CASE("ExecuteSQL uses statement in content", "[ExecuteSQL3]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}});

  controller.insertValues({{11, "one"}, {22, "two"}});

  auto input_file = plan->addInput({}, "SELECT * FROM test_table ORDER BY int_col ASC");

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(minifi::processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");

  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{
      "int_col": 11,
      "text_col": "one"
    },{
      "int_col": 22,
      "text_col": "two"
    }]
  )");
}

TEST_CASE("ExecuteSQL uses sql.args.N.value attributes", "[ExecuteSQL4]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}});

  controller.insertValues({{11, "apple"}, {11, "banana"}, {22, "banana"}});

  auto input_file = plan->addInput({
    {"sql.args.1.value", "11"},
    {"sql.args.2.value", "banana"}
  }, "SELECT * FROM test_table WHERE int_col = ? AND text_col = ?");

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(minifi::processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "1");

  auto content = plan->getContent(flow_files[0]);
  utils::verifyJSON(content, R"(
    [{
      "int_col": 11,
      "text_col": "banana"
    }]
  )");
}

TEST_CASE("ExecuteSQL honors Max Rows Per Flow File", "[ExecuteSQL5]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}});
  auto sql_proc = plan->getSQLProcessor();
  sql_proc->setProperty(minifi::processors::ExecuteSQL::MaxRowsPerFlowFile.name, "2");
  sql_proc->setProperty(minifi::processors::ExecuteSQL::SQLSelectQuery.name, "SELECT text_col FROM test_table ORDER BY int_col ASC");

  controller.insertValues({
    {101, "apple"},
    {102, "banana"},
    {103, "pear"},
    {104, "strawberry"},
    {105, "pineapple"}
  });

  auto input_file = plan->addInput();

  plan->run();

  auto content_verifier = [&] (const std::shared_ptr<core::FlowFile>& actual, const std::string& expected) {
    utils::verifyJSON(plan->getContent(actual), expected);
  };

  FlowFileMatcher matcher{content_verifier, {
      minifi::processors::ExecuteSQL::RESULT_ROW_COUNT,
      minifi::processors::ExecuteSQL::FRAGMENT_COUNT,
      minifi::processors::ExecuteSQL::FRAGMENT_INDEX,
      minifi::processors::ExecuteSQL::FRAGMENT_IDENTIFIER
  }};

  std::optional<std::string> fragment_id;

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 3);
  matcher.verify(flow_files[0],
    { AttributeValue("2"), AttributeValue("3"), AttributeValue("0"), capture(fragment_id) },
    R"([{"text_col": "apple"}, {"text_col": "banana"}])");
  REQUIRE(fragment_id);
  matcher.verify(flow_files[1],
    { AttributeValue("2"), AttributeValue("3"), AttributeValue("1"), AttributeValue(*fragment_id) },
    R"([{"text_col": "pear"}, {"text_col": "strawberry"}])");
  matcher.verify(flow_files[2],
    { AttributeValue("1"), AttributeValue("3"), AttributeValue("2"), AttributeValue(*fragment_id) },
    R"([{"text_col": "pineapple"}])");
}

TEST_CASE("ExecuteSQL incoming flow file is malformed", "[ExecuteSQL6]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}, {"failure", "d"}});

  std::shared_ptr<minifi::core::FlowFile> input_file;
  SECTION("Invalid sql statement") {
    input_file = plan->addInput({}, "not a valid sql statement");
  }
  SECTION("No such table") {
    input_file = plan->addInput({}, "SELECT * FROM no_such_table;");
  }

  plan->run();

  REQUIRE(plan->getOutputs({"success", "d"}).empty());
  auto output = plan->getOutputs({"failure", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);
}

TEST_CASE("ExecuteSQL select query is malformed", "[ExecuteSQL7]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}, {"failure", "d"}});
  plan->getSQLProcessor()->setProperty(minifi::processors::ExecuteSQL::SQLSelectQuery.name, "SELECT * FROM test_table WHERE int_col = ?;");

  std::shared_ptr<minifi::core::FlowFile> input_file;
  SECTION("Less than required arguments") {
    input_file = plan->addInput({}, "ignored content");
  }
// TODO(MINIFICPP-2001):
//  SECTION("More than required arguments") {
//    input_file = plan->addInput({
//      {"sql.args.1.value", "1"},
//      {"sql.args.2.value", "2"}
//    }, "ignored content");
//  }

  plan->run();

  REQUIRE(plan->getOutputs({"success", "d"}).empty());
  auto output = plan->getOutputs({"failure", "d"});
  REQUIRE(output.size() == 1);
  REQUIRE(output.at(0) == input_file);
}

}  // namespace org::apache::nifi::minifi::test
