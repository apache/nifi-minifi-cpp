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
#include "data/SQLRowsetProcessor.h"
#include "data/WriteCallback.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string ExecuteSQL::ProcessorName("ExecuteSQL");

const core::Property ExecuteSQL::s_dbControllerService(
    core::PropertyBuilder::createProperty("DB Controller Service")->isRequired(true)->withDescription("Database Controller Service.")->supportsExpressionLanguage(true)->build());

const core::Property ExecuteSQL::s_sqlSelectQuery(
  core::PropertyBuilder::createProperty("SQL select query")->isRequired(true)->withDescription(
    "The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. "
    "Note that Expression Language is not evaluated for flow file contents.")->supportsExpressionLanguage(true)->build());

const core::Property ExecuteSQL::s_maxRowsPerFlowFile(
	core::PropertyBuilder::createProperty("Max Rows Per Flow File")->isRequired(true)->withDefaultValue<int>(0)->withDescription(
		"The maximum number of result rows that will be included intoi a flow file. If zero then all will be placed into the flow file")->supportsExpressionLanguage(true)->build());

const core::Relationship ExecuteSQL::s_success("success", "Successfully created FlowFile from SQL query result set.");

static const std::string ResultRowCount = "executesql.row.count";

ExecuteSQL::ExecuteSQL(const std::string& name, utils::Identifier uuid)
    : core::Processor(name, uuid), max_rows_(0),
      logger_(logging::LoggerFactory<ExecuteSQL>::getLogger()) {
}

ExecuteSQL::~ExecuteSQL() {
}

void ExecuteSQL::initialize() {
  //! Set the supported properties
  setSupportedProperties( { s_dbControllerService, s_sqlSelectQuery, s_maxRowsPerFlowFile });

  //! Set the supported relationships
  setSupportedRelationships( { s_success });
}

void ExecuteSQL::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(s_dbControllerService.getName(), db_controller_service_);
  context->getProperty(s_sqlSelectQuery.getName(), sqlSelectQuery_);
  context->getProperty(s_maxRowsPerFlowFile.getName(), max_rows_);

  database_service_ = std::dynamic_pointer_cast<sql::controllers::DatabaseService>(context->getControllerService(db_controller_service_));
  if (!database_service_)
    throw minifi::Exception(PROCESSOR_EXCEPTION, "'DB Controller Service' must be defined");
}

void ExecuteSQL::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
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

  try {
    auto statement = connection_->prepareStatement(sqlSelectQuery_);

    auto rowset = statement->execute();

    int count = 0;
    size_t rowCount = 0;
    sql::JSONSQLWriter jsonSQLWriter;
    sql::SQLRowsetProcessor sqlRowsetProcessor(rowset, { &jsonSQLWriter });

    // Process rowset.
    do {
      rowCount = sqlRowsetProcessor.process(max_rows_ == 0 ? std::numeric_limits<size_t>::max() : max_rows_);
      count++;
      if (rowCount == 0)
        break;

      const auto& output = jsonSQLWriter.toString();
      if (!output.empty()) {
        WriteCallback writer(output.data(), output.size());
        auto newflow = session->create();
        newflow->addAttribute(ResultRowCount, std::to_string(rowCount));
        session->write(newflow, &writer);
        session->transfer(newflow, s_success);
      }
    } while (rowCount > 0);
  } catch (std::exception& e) {
    logger_->log_error(e.what());
    throw;
  }
}

void ExecuteSQL::notifyStop() {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
