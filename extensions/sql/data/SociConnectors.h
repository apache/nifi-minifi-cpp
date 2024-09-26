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
#include <ctime>

#include "Exception.h"
#include "data/DatabaseConnectors.h"
#include <soci/soci.h>
#include <soci/odbc/soci-odbc.h>
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::sql {

class SociRow : public Row {
 public:
  void setIterator(const soci::rowset<soci::row>::iterator& iter);
  soci::rowset<soci::row>::iterator getIterator() const;
  void next();

  std::size_t size() const override;
  std::string getColumnName(std::size_t index) const override;
  bool isNull(std::size_t index) const override;
  DataType getDataType(std::size_t index) const override;
  std::string getString(std::size_t index) const override;
  double getDouble(std::size_t index) const override;
  int getInteger(std::size_t index) const override;
  long long getLongLong(std::size_t index) const override;
  unsigned long long getUnsignedLongLong(std::size_t index) const override;
  std::tm getDate(std::size_t index) const override;

 private:
  soci::rowset<soci::row>::iterator current_;
};

class SociRowset : public Rowset {
 public:
  SociRowset(const soci::rowset<soci::row>& rowset) : rowset_(rowset) {
  }

  void reset() override;
  bool is_done() override;
  Row& getCurrent() override;
  void next() override;

 private:
  soci::rowset<soci::row> rowset_;
  SociRow current_row_;
};

class SociStatement : public Statement {
 public:
  SociStatement(soci::session& session, const std::string &query);

  std::unique_ptr<Rowset> execute(const std::vector<std::string>& args = {}) override;

 protected:
  soci::session& session_;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};

class SociSession : public Session {
 public:
  explicit SociSession(soci::session& session)
    : session_(session) {
  }

  void begin() override;
  void commit() override;
  void rollback() override;
  void execute(const std::string &statement) override;

protected:
  soci::session& session_;
};

class ODBCConnection : public sql::Connection {
 public:
  explicit ODBCConnection(std::string connectionString);

  bool connected(std::string& exception) const override;
  std::unique_ptr<sql::Statement> prepareStatement(const std::string& query) const override;
  std::unique_ptr<Session> getSession() const override;

 private:
   soci::connection_parameters getSessionParameters() const;

 private:
  std::unique_ptr<soci::session> session_;
  std::string connection_string_;
};

}  // namespace org::apache::nifi::minifi::sql
