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

#include "SQLTestController.h"
#include "processors/ExecuteSQL.h"
#include "Utils.h"
#include "FlowFileMatcher.h"

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
  flow_files[0]->getAttribute(processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");

  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
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
  sql_proc->setProperty("SQL select query", "SELECT * FROM test_table WHERE int_col == ${int_col_value}");

  controller.insertValues({{11, "one"}, {22, "two"}});

  auto input_file = plan->addInput({{"int_col_value", "11"}});

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "1");

  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
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
  flow_files[0]->getAttribute(processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "2");

  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
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
  }, "SELECT * FROM test_table WHERE int_col == ? AND text_col == ?");

  plan->run();

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "1");

  auto content = plan->getContent(flow_files[0]);
  verifyJSON(content, R"(
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
  sql_proc->setProperty(processors::ExecuteSQL::MaxRowsPerFlowFile.getName(), "2");
  sql_proc->setProperty(processors::ExecuteSQL::SQLSelectQuery.getName(), "SELECT text_col FROM test_table ORDER BY int_col ASC");

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
    verifyJSON(plan->getContent(actual), expected);
  };

  FlowFileMatcher matcher{content_verifier, {
      processors::ExecuteSQL::RESULT_ROW_COUNT,
      processors::ExecuteSQL::FRAGMENT_COUNT,
      processors::ExecuteSQL::FRAGMENT_INDEX,
      processors::ExecuteSQL::FRAGMENT_IDENTIFIER
  }};

  utils::optional<std::string> fragment_id;

  auto flow_files = plan->getOutputs({"success", "d"});
  REQUIRE(flow_files.size() == 3);
  matcher.verify(flow_files[0],
    {"2", "3", "0", var("frag_id")},
    R"([{"text_col": "apple"}, {"text_col": "banana"}])");
  matcher.verify(flow_files[1],
    {"2", "3", "1", var("frag_id")},
    R"([{"text_col": "pear"}, {"text_col": "strawberry"}])");
  matcher.verify(flow_files[2],
    {"1", "3", "2", var("frag_id")},
    R"([{"text_col": "pineapple"}])");
}

TEST_CASE("ExecuteSQL incoming flow file is malformed", "[ExecuteSQL6]") {
  SQLTestController controller;

  auto plan = controller.createSQLPlan("ExecuteSQL", {{"success", "d"}, {"failure", "d"}});

  auto input_file = plan->addInput({}, "not a valid sql statement");

  REQUIRE_THROWS(plan->run());
}
