// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "SQLProcessor.h"

#include <vector>
#include <memory>

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/Exception.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void SQLProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  db_service_ = utils::parseControllerService<sql::controllers::DatabaseService>(context, DBControllerService, getUUID());
  processOnSchedule(context);
}

void SQLProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  try {
    if (!connection_) {
      connection_ = db_service_->getConnection();
    }
    if (!connection_) {
      throw sql::ConnectionError("Could not establish sql connection");
    }
    processOnTrigger(context, session);
  } catch (const sql::ConnectionError& ex) {
    logger_->log_error("Connection error: {}", ex.what());
    // try to reconnect next time
    connection_.reset();
    throw;
  }
}

std::vector<std::string> SQLProcessor::collectArguments(const std::shared_ptr<core::FlowFile> &flow_file) {
  if (!flow_file) {
    return {};
  }
  std::vector<std::string> arguments;
  for (size_t arg_idx{1};; ++arg_idx) {
    std::string arg;
    if (!flow_file->getAttribute("sql.args." + std::to_string(arg_idx) + ".value", arg)) {
      break;
    }
    arguments.push_back(std::move(arg));
  }
  return arguments;
}

}  // namespace org::apache::nifi::minifi::processors
