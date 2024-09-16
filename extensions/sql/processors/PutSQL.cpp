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

#include "PutSQL.h"

#include <string>
#include <utility>

#include "io/BufferStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Exception.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::processors {

PutSQL::PutSQL(std::string_view name, const utils::Identifier& uuid)
  : SQLProcessor(name, uuid, core::logging::LoggerFactory<PutSQL>::getLogger(uuid)) {
}

void PutSQL::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutSQL::processOnSchedule(core::ProcessContext& context) {
  if (auto sql_statement = context.getProperty(SQLStatement); sql_statement && sql_statement->empty()) {
    throw Exception(PROCESSOR_EXCEPTION, "Empty SQL statement");
  }
}

void PutSQL::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  std::string sql_statement = context.getProperty(SQLStatement).value_or("");
  if (sql_statement.empty()) {
    logger_->log_debug("Using the contents of the flow file as the SQL statement");
    sql_statement = to_string(session.readBuffer(flow_file));
  }

  try {
    connection_->prepareStatement(sql_statement)->execute(collectArguments(flow_file));
    session.transfer(flow_file, Success);
  } catch (const sql::StatementError& ex) {
    logger_->log_error("Error while executing SQL statement in flow file: {}", ex.what());
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(PutSQL, Processor);

}  // namespace org::apache::nifi::minifi::processors
