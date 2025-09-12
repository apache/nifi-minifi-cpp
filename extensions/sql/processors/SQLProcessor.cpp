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
