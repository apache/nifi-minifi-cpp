/**
 * @file ExecuteSQL.cpp
 * ExecuteSQL class declaration
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

#include "ExecuteSQL.h"

#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <memory>
#include <codecvt>

#include <soci/soci.h>

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"
#include "utils/OsUtils.h"
#include "data/DatabaseConnectors.h"
#include "data/JSONSQLWriter.h"
#include "data/SQLRowsetProcessor.h"
#include "data/WriteCallback.h"
#include "utils/ValueParser.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string ExecuteSQL::ProcessorName("ExecuteSQL");

const core::Property ExecuteSQL::SQLSelectQuery(
  core::PropertyBuilder::createProperty("SQL select query")
  ->isRequired(true)
  ->withDescription(
    "The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. "
    "Note that Expression Language is not evaluated for flow file contents.")
  ->supportsExpressionLanguage(true)->build());

const core::Relationship ExecuteSQL::Success("success", "Successfully created FlowFile from SQL query result set.");

const std::string ExecuteSQL::RESULT_ROW_COUNT = "executesql.row.count";

ExecuteSQL::ExecuteSQL(const std::string& name, utils::Identifier uuid)
  : SQLProcessor(name, uuid, logging::LoggerFactory<ExecuteSQL>::getLogger()) {
}

void ExecuteSQL::initialize() {
  //! Set the supported properties
  setSupportedProperties({ DBControllerService, OutputFormat, SQLSelectQuery, MaxRowsPerFlowFile});

  //! Set the supported relationships
  setSupportedRelationships({ Success });
}

void ExecuteSQL::processOnSchedule(core::ProcessContext& context) {
  context.getProperty(OutputFormat.getName(), output_format_);

  max_rows_ = [&] {
    uint64_t max_rows;
    context.getProperty(MaxRowsPerFlowFile.getName(), max_rows);
    return gsl::narrow<size_t>(max_rows);
  }();
}

void ExecuteSQL::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (flow_file) {
    session.remove(flow_file);
  }

  std::string query;
  context.getProperty(SQLSelectQuery, query, flow_file);

  auto statement = connection_->prepareStatement(query);

  auto row_set = statement->execute();

  sql::JSONSQLWriter sqlWriter(output_format_ == OutputType::JSONPretty);
  sql::SQLRowsetProcessor sqlRowsetProcessor(row_set, { &sqlWriter });

  // Process rowset.
  while (size_t row_count = sqlRowsetProcessor.process(max_rows_)) {
    WriteCallback writer(sqlWriter.toString());
    auto new_flow = session.create();
    new_flow->addAttribute(RESULT_ROW_COUNT, std::to_string(row_count));
    session.write(new_flow, &writer);
    session.transfer(new_flow, Success);
  }
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
