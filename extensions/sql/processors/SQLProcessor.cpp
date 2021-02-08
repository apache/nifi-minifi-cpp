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

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const core::Property SQLProcessor::DBControllerService(
    core::PropertyBuilder::createProperty("DB Controller Service")
    ->isRequired(true)
    ->withDescription("Database Controller Service.")
    ->supportsExpressionLanguage(true)->build());

void SQLProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  std::string controllerService;
  context->getProperty(DBControllerService.getName(), controllerService);

  dbService_ = std::dynamic_pointer_cast<sql::controllers::DatabaseService>(context->getControllerService(controllerService));
  if (!dbService_) {
    throw minifi::Exception(PROCESSOR_EXCEPTION, "'DB Controller Service' must be defined");
  }

  processOnSchedule(*context);
}

void SQLProcessor::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  std::lock_guard<std::mutex> guard(onTriggerMutex_);

  try {
    if (!connection_) {
      connection_ = dbService_->getConnection();
    }
    processOnTrigger(*context, *session);
  } catch (const std::exception& e) {
    logger_->log_error("SQLProcessor: '%s'", e.what());
    if (connection_) {
      std::string exp;
      if (!connection_->connected(exp)) {
        logger_->log_error("SQLProcessor: Connection exception: %s", exp.c_str());
        connection_.reset();
      }
    }
    context->yield();
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

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
