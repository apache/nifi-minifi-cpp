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

#ifndef EXTENSIONS_SQL_SERVICES_DATABASECONNECTORS_H_
#define EXTENSIONS_SQL_SERVICES_DATABASECONNECTORS_H_

#include <memory>
#include <iostream>
#include <algorithm>
#include <cctype>

#include <soci/soci.h>

#include "Utils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

/**
 * We do not intend to create an abstract facade here. We know that SOCI is the underlying
 * SQL library. We only wish to abstract ODBC specific information
 */

class Statement {
 public:

  explicit Statement(const std::unique_ptr<soci::session>& session, const std::string &query)
    : session_(session), query_(query) {
  }

  virtual ~Statement() {
  }

  soci::rowset<soci::row> execute() {
    return session_->prepare << query_;
  }

 protected:
  std::string query_;
  const std::unique_ptr<soci::session>& session_;
};

class Session {
 public:

  explicit Session(const std::unique_ptr<soci::session>& session)
    : session_(session) {
  }

  virtual ~Session() {
  }

  void begin() {
    session_->begin();
  }

  void commit() {
    session_->commit();
  }

  void rollback() {
    session_->rollback();
  }

  void execute(const std::string &statement) {
    *session_ << statement;
  }

protected:
  const std::unique_ptr<soci::session>& session_;
};

class Connection {
 public:
  virtual ~Connection() {
  }
  virtual bool ok(std::string& exception) const = 0;
  virtual std::unique_ptr<Statement> prepareStatement(const std::string &query) const = 0;
  virtual std::unique_ptr<Session> getSession() const = 0;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_SQL_SERVICES_DATABASECONNECTORS_H_ */
