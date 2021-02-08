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

#include <string>
#include <memory>

#include <soci/soci.h>

#include "io/BufferStream.h"
#include "io/StreamPipe.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"
#include "data/JSONSQLWriter.h"
#include "data/SQLRowsetProcessor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string ExecuteSQL::ProcessorName("ExecuteSQL");

const core::Property ExecuteSQL::SQLSelectQuery(
  core::PropertyBuilder::createProperty("SQL select query")
  ->withDescription(
    "The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. "
    "Note that Expression Language is not evaluated for flow file contents.")
  ->supportsExpressionLanguage(true)->build());

const core::Relationship ExecuteSQL::Success("success", "Successfully created FlowFile from SQL query result set.");
const core::Relationship ExecuteSQL::Failure("failure", "SQL query execution failed. Incoming FlowFile will be penalized and routed to this relationship.");

const std::string ExecuteSQL::RESULT_ROW_COUNT = "executesql.row.count";
const std::string ExecuteSQL::INPUT_FLOW_FILE_UUID = "input.flowfile.uuid";

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
  auto input_flow_file = session.get();

  try {
    std::string query;
    if (!context.getProperty(SQLSelectQuery, query, input_flow_file)) {
      if (!input_flow_file) {
        throw Exception(PROCESSOR_EXCEPTION,
                        "No incoming FlowFile and the \"SQL select query\" processor property is not specified");
      }
      logger_->log_debug("Using the contents of the flow file as the SQL statement");
      auto buffer = std::make_shared<io::BufferStream>();
      InputStreamPipe content_reader{buffer};
      session.read(input_flow_file, &content_reader);
      query = std::string{reinterpret_cast<const char *>(buffer->getBuffer()), buffer->size()};
    }
    if (query.empty()) {
      throw Exception(PROCESSOR_EXCEPTION, "Empty SQL statement");
    }

    auto row_set = connection_->prepareStatement(query)->execute(collectArguments(input_flow_file));

    sql::JSONSQLWriter sqlWriter{output_format_ == OutputType::JSONPretty};
    FlowFileGenerator flow_file_creator{session, sqlWriter};
    sql::SQLRowsetProcessor sqlRowsetProcessor(row_set, {sqlWriter, flow_file_creator});

    // Process rowset.
    while (size_t row_count = sqlRowsetProcessor.process(max_rows_)) {
      auto new_file = flow_file_creator.getLastFlowFile();
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
  } catch (const std::exception&) {
    // sql execution failed, transfer to failure
    if (input_flow_file) {
      session.transfer(input_flow_file, Failure);
    }
    throw;
  }
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
