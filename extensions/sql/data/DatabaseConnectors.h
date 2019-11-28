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
#include <soci.h>

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

  explicit Statement(std::unique_ptr<soci::session>& sql, const std::string &query)
      : sql_(std::move(sql)),
        query_(query) {
  }

  virtual ~Statement() {
  }

  soci::rowset<soci::row> execute(){
    return sql_->prepare << query_;
  }

 protected:

  std::string query_;
  std::unique_ptr<soci::session> sql_;
};

class Session {
 public:

  explicit Session(std::unique_ptr<soci::session>& sql)
    : sql_(std::move(sql)) {
  }

  virtual ~Session() {
  }

  void begin() {
    sql_->begin();
  }

  void commit() {
    sql_->commit();
  }

  void rollback() {
    sql_->rollback();
  }

  void execute(const std::string &statement) {
    *sql_ << statement;
  }

protected:
  std::unique_ptr<soci::session> sql_;
};

class Connection {
 public:
  virtual ~Connection() {
  }
  virtual std::unique_ptr<Statement> prepareStatement(const std::string &query) const = 0;
  virtual std::unique_ptr<Session> getSession() const = 0;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_SQL_SERVICES_DATABASECONNECTORS_H_ */
