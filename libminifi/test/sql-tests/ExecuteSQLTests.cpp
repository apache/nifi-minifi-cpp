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
#include "rapidjson/document.h"

TEST_CASE("ExecuteSQL property", "[ExecuteSQL1]") {
  SQLTestController controller;

  auto plan = controller.createPlan();
  controller.initSQLService(plan);

  controller.insertValues({{11, "one"}, {22, "two"}});

  plan->addProcessor("GenerateFlowFile", "Gen");
  auto update = plan->addProcessor("UpdateAttribute", "Update", {{"success", "d"}}, true);
  plan->setProperty(update, "int_col_value", "11", true);
  auto sql_proc = plan->addProcessor("ExecuteSQL", "SQL", {{"success", "d"}}, true);
  plan->setProperty(sql_proc, "SQL select query", "SELECT * FROM test_table WHERE int_col == ${int_col_value}");
  plan->setProperty(sql_proc, "DB Controller Service", "ODBCService");
  auto output = plan->addConnection(sql_proc, {"success", "d"}, {});

  plan->runNextProcessor();  // GenerateFlowFile
  plan->runNextProcessor();  // UpdateAttribute
  plan->runNextProcessor();  // ExecuteSQL

  auto flow_files = controller.pollAll(output);
  REQUIRE(flow_files.size() == 1);
  std::string row_count;
  flow_files[0]->getAttribute(processors::ExecuteSQL::RESULT_ROW_COUNT, row_count);
  REQUIRE(row_count == "1");

  auto content = controller.getContent(plan, flow_files[0]);
  rapidjson::Document doc;
  rapidjson::ParseResult result = doc.Parse(content.c_str());
  REQUIRE(result);
  REQUIRE(doc.IsArray());
  REQUIRE(doc.Size() == 1);
  REQUIRE(doc[0].IsObject());
  REQUIRE(doc[0]["int_col"] == 11);
  REQUIRE(doc[0]["text_col"] == "one");
}

