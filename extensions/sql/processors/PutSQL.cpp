/**
 * @file PutSQL.cpp
 * PutSQL class declaration
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

#include "PutSQL.h"

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

#include "io/DataStream.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"
#include "utils/OsUtils.h"
#include "data/DatabaseConnectors.h"
#include "data/JSONSQLWriter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string PutSQL::ProcessorName("PutSQL");

const core::Property PutSQL::s_dbControllerService(
  core::PropertyBuilder::createProperty("DB Controller Service")->isRequired(true)->withDescription("Database Controller Service.")->supportsExpressionLanguage(true)->build());

const core::Property PutSQL::s_sqlStatements(
  core::PropertyBuilder::createProperty("SQL statements")->isRequired(true)->withDefaultValue("System")->withDescription(
    "A semicolon-delimited list of SQL statements to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL statements, to be issued by the processor to the database.")
    ->supportsExpressionLanguage(true)->build());

const core::Relationship PutSQL::s_success("success", "Database is successfully updated.");

PutSQL::PutSQL(const std::string& name, utils::Identifier uuid)
    : core::Processor(name, uuid), logger_(logging::LoggerFactory<PutSQL>::getLogger()) {
}

PutSQL::~PutSQL() {
}

void PutSQL::initialize() {
  //! Set the supported properties
  setSupportedProperties( { s_dbControllerService, s_sqlStatements });

  //! Set the supported relationships
  setSupportedRelationships( { s_success });
}

void PutSQL::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(s_dbControllerService.getName(), db_controller_service_);

  std::string sqlStatements;
  context->getProperty(s_sqlStatements.getName(), sqlStatements);
  sqlStatements_ = utils::StringUtils::split(sqlStatements, ";");

  database_service_ = std::dynamic_pointer_cast<sql::controllers::DatabaseService>(context->getControllerService(db_controller_service_));
  if (!database_service_) 
    throw minifi::Exception(PROCESSOR_EXCEPTION, "'DB Controller Service' must be defined");
}

void PutSQL::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("'onTrigger' is called before previous 'onTrigger' call is finished.");
    context->yield();
    return;
  }

  if (!connection_) {
    connection_ = database_service_->getConnection();
    if (!connection_) {
      context->yield();
      return;
    }
  }

  const auto dbSession = connection_->getSession();

  try {
    dbSession->begin();
    for (const auto& statement : sqlStatements_) {
      dbSession->execute(statement);
    }
    dbSession->commit();
  } catch (std::exception& e) {
    logger_->log_error("SQL statement error: %s", e.what());
    dbSession->rollback();
    throw;
  }
}

void PutSQL::notifyStop() {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
