/**
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

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"

#include "utils/GeneralUtils.h"
#include "DatabaseService.h"
#include "core/Resource.h"
#include "data/DatabaseConnectors.h"
#include <memory>
#include <unordered_map>

#include <soci/soci.h>
#include <soci/odbc/soci-odbc.h>

#include <iostream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {
namespace controllers {

class ODBCConnection : public sql::Connection {
 public:
  explicit ODBCConnection(const std::string& connectionString)
    : connection_string_(connectionString) {
      session_ = utils::make_unique<soci::session>(getSessionParameters());
  }

  virtual ~ODBCConnection() = default;

  bool connected(std::string& exception) const override {
    try {
      exception.clear();
      // According to https://stackoverflow.com/questions/3668506/efficient-sql-test-query-or-validation-query-that-will-work-across-all-or-most by Rob Hruska, 
      // 'select 1' works for: H2, MySQL, Microsoft SQL Server, PostgreSQL, SQLite. For Oracle 'SELECT 1 FROM DUAL' works.
      prepareStatement("select 1")->execute();
      return true;
    } catch (std::exception& e) {
      exception = e.what();
      return false;
    }
  }

  std::unique_ptr<sql::Statement> prepareStatement(const std::string& query) const override {
    return utils::make_unique<sql::Statement>(*session_, query);
  }

  std::unique_ptr<Session> getSession() const override {
    return utils::make_unique<sql::Session>(*session_);
  }

 private:
   soci::connection_parameters getSessionParameters() const {
     static const soci::backend_factory &backEnd = *soci::factory_odbc();

     soci::connection_parameters parameters(backEnd, connection_string_);
     parameters.set_option(soci::odbc_option_driver_complete, "0" /* SQL_DRIVER_NOPROMPT */);

     return parameters;
   }

 private:
  std::unique_ptr<soci::session> session_;
  std::string connection_string_;
};

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide Database services
 */
class ODBCService : public DatabaseService {
 public:
  explicit ODBCService(const std::string &name, utils::Identifier uuid = utils::Identifier())
    : DatabaseService(name, uuid),
      logger_(logging::LoggerFactory<ODBCService>::getLogger()) {
    initialize();
  }

  explicit ODBCService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : DatabaseService(name),
        logger_(logging::LoggerFactory<ODBCService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  virtual std::unique_ptr<sql::Connection> getConnection() const override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(ODBCService, "Controller service that provides ODBC database connection");

} /* namespace controllers */
} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
