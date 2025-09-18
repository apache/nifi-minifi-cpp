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
#include <utility>

#include "io/StreamPipe.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/Exception.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const std::string ExecuteSQL::RESULT_ROW_COUNT = "executesql.row.count";
const std::string ExecuteSQL::INPUT_FLOW_FILE_UUID = "input.flowfile.uuid";

void ExecuteSQL::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ExecuteSQL::processOnSchedule(core::ProcessContext& context) {
  output_format_ = utils::parseEnumProperty<flow_file_source::OutputType>(context, OutputFormat);
  max_rows_ = gsl::narrow<size_t>(utils::parseU64Property(context, MaxRowsPerFlowFile));
}

void ExecuteSQL::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_flow_file = session.get();

  auto query = context.getProperty(SQLSelectQuery, input_flow_file.get());
  if (!query) {
    if (!input_flow_file) {
      throw Exception(PROCESSOR_EXCEPTION,
                      "No incoming FlowFile and the \"" + std::string{SQLSelectQuery.name} + "\" processor property is not specified");  // NOLINT(whitespace/braces)
    }
    logger_->log_debug("Using the contents of the flow file as the SQL statement");
    std::string buffer_str = to_string(session.readBuffer(input_flow_file));
    query = buffer_str;
  }
  if (query->empty()) {
    logger_->log_error("Empty sql statement");
    if (input_flow_file) {
      session.transfer(input_flow_file, Failure);
      return;
    }
    throw Exception(PROCESSOR_EXCEPTION, "Empty SQL statement");
  }

  std::unique_ptr<sql::Rowset> row_set;
  try {
    row_set = connection_->prepareStatement(*query)->execute(collectArguments(input_flow_file));
  } catch (const sql::StatementError& ex) {
    logger_->log_error("Error while executing sql statement: {}", ex.what());
    session.transfer(input_flow_file, Failure);
    return;
  }

  sql::JSONSQLWriter json_writer{output_format_ == flow_file_source::OutputType::JSONPretty};
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

REGISTER_RESOURCE(ExecuteSQL, Processor);

}  // namespace org::apache::nifi::minifi::processors
