// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PutSQL.h"

#include <string>
#include <utility>

#include "io/BufferStream.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/Exception.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::processors {

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
