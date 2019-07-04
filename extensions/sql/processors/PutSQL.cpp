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

const core::Property PutSQL::s_sqlStatements(
  core::PropertyBuilder::createProperty("SQL statements")->isRequired(true)->withDefaultValue("System")->withDescription(
    "A semicolon-delimited list of SQL statements to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL statements, to be issued by the processor to the database.")
    ->supportsExpressionLanguage(true)->build());

const core::Relationship PutSQL::s_success("success", "Database is successfully updated.");

PutSQL::PutSQL(const std::string& name, utils::Identifier uuid)
  : SQLProcessor(name, uuid) {
}

PutSQL::~PutSQL() {
}

void PutSQL::initialize() {
  //! Set the supported properties
  setSupportedProperties( { dbControllerService(), s_sqlStatements });

  //! Set the supported relationships
  setSupportedRelationships( { s_success });
}

void PutSQL::processOnSchedule(core::ProcessContext& context) {
  std::string sqlStatements;
  context.getProperty(s_sqlStatements.getName(), sqlStatements);
  sqlStatements_ = utils::StringUtils::split(sqlStatements, ";");
}

void PutSQL::processOnTrigger(core::ProcessSession& session) {
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
