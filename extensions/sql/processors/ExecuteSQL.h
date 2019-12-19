/**
 * @file ExecuteSQL.h
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

#pragma once

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "services/DatabaseService.h"
#include <sstream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! ExecuteSQL Class
class ExecuteSQL : public core::Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  explicit ExecuteSQL(const std::string& name, utils::Identifier uuid = utils::Identifier());

  //! Destructor
  virtual ~ExecuteSQL();

  //! Processor Name
  static const std::string ProcessorName;

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  
  void initialize(void) override;
  void notifyStop() override;

  static const core::Property s_dbControllerService;
  static const core::Property s_sqlSelectQuery;
  static const core::Property s_maxRowsPerFlowFile;

  static const core::Relationship s_success;

 private:
  std::shared_ptr<sql::controllers::DatabaseService> database_service_;
  // Logger
  int max_rows_;
  std::shared_ptr<logging::Logger> logger_;
  std::string db_controller_service_;
  std::string sqlSelectQuery_;
  std::unique_ptr<sql::Connection> connection_;
  std::mutex onTriggerMutex_;
};

REGISTER_RESOURCE(ExecuteSQL, "ExecuteSQL to execute SELECT statement via ODBC.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
