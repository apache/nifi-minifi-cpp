/**
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

#include <string>
#include <memory>

#include <soci/soci.h>

#include "io/BufferStream.h"
#include "io/StreamPipe.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Exception.h"
#include "data/JSONSQLWriter.h"
#include "data/SQLRowsetProcessor.h"

namespace org::apache::nifi::minifi::processors {

const std::string ExecuteSQL::RESULT_ROW_COUNT = "executesql.row.count";
const std::string ExecuteSQL::INPUT_FLOW_FILE_UUID = "input.flowfile.uuid";

ExecuteSQL::ExecuteSQL(std::string name, const utils::Identifier& uuid)
  : SQLProcessor(std::move(name), uuid, core::logging::LoggerFactory<ExecuteSQL>::getLogger()) {
}

void ExecuteSQL::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExecuteSQL::processOnSchedule(core::ProcessContext& context) {
  context.getProperty(OutputFormat.getName(), output_format_);

  max_rows_ = [&] {
    uint64_t max_rows = 0;
    context.getProperty(MaxRowsPerFlowFile.getName(), max_rows);
    return gsl::narrow<size_t>(max_rows);
  }();
}

void ExecuteSQL::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_flow_file = session.get();

  std::string query;
  if (!context.getProperty(SQLSelectQuery, query, input_flow_file)) {
    if (!input_flow_file) {
      throw Exception(PROCESSOR_EXCEPTION,
                      "No incoming FlowFile and the \"" + SQLSelectQuery.getName() + "\" processor property is not specified");
    }
    logger_->log_debug("Using the contents of the flow file as the SQL statement");
    query = to_string(session.readBuffer(input_flow_file));
  }
  if (query.empty()) {
    throw Exception(PROCESSOR_EXCEPTION, "Empty SQL statement");
  }

  auto row_set = connection_->prepareStatement(query)->execute(collectArguments(input_flow_file));

  sql::JSONSQLWriter json_writer{output_format_ == OutputType::JSONPretty};
  FlowFileGenerator flow_file_creator{session, json_writer};
  sql::SQLRowsetProcessor sql_rowset_processor(std::move(row_set), {json_writer, flow_file_creator});

  // Process rowset.
  while (size_t row_count = sql_rowset_processor.process(max_rows_)) {
    auto new_file = flow_file_creator.getLastFlowFile();
    gsl_Expects(new_file);
    new_file->addAttribute(RESULT_ROW_COUNT, std::to_string(row_count));
    if (input_flow_file) {
      new_file->addAttribute(INPUT_FLOW_FILE_UUID, input_flow_file->getUUIDStr());
    }
  }

  // transfer flow files
  if (input_flow_file) {
    session.remove(input_flow_file);
  }
  for (const auto& new_file : flow_file_creator.getFlowFiles()) {
    session.transfer(new_file, Success);
  }
}

}  // namespace org::apache::nifi::minifi::processors
