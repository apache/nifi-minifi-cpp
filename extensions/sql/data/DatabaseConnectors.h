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

#include <memory>
#include <string>

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

  explicit Statement(soci::session& session, const std::string &query)
    : session_(session), query_(query) {
  }

  virtual ~Statement() = default;

  soci::rowset<soci::row> execute(const std::vector<std::string>& args = {}) {
    auto stmt = session_.prepare << query_;
    for (auto& arg : args) {
      // binds arguments to the prepared statement
      stmt.operator,(soci::use(arg));
    }
    return stmt;
  }

 protected:
  soci::session& session_;
  std::string query_;
};

class Session {
 public:

  explicit Session(soci::session& session)
    : session_(session) {
  }

  virtual ~Session() = default;

  void begin() {
    session_.begin();
  }

  void commit() {
    session_.commit();
  }

  void rollback() {
    session_.rollback();
  }

  void execute(const std::string &statement) {
    session_ << statement;
  }

protected:
  soci::session& session_;
};

class Connection {
 public:
  virtual ~Connection() = default;
  virtual bool connected(std::string& exception) const = 0;
  virtual std::unique_ptr<Statement> prepareStatement(const std::string &query) const = 0;
  virtual std::unique_ptr<Session> getSession() const = 0;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

