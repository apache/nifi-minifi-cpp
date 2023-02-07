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
#include <vector>
#include <ctime>
#include <stdexcept>

namespace org::apache::nifi::minifi::sql {

enum class DataType {
  STRING,
  DOUBLE,
  INTEGER,
  LONG_LONG,
  UNSIGNED_LONG_LONG,
  DATE
};

class Row {
 public:
  virtual ~Row() = default;
  virtual std::size_t size() const = 0;
  virtual std::string getColumnName(std::size_t index) const = 0;
  virtual bool isNull(std::size_t index) const = 0;
  virtual DataType getDataType(std::size_t index) const = 0;
  virtual std::string getString(std::size_t index) const = 0;
  virtual double getDouble(std::size_t index) const = 0;
  virtual int getInteger(std::size_t index) const = 0;
  virtual long long getLongLong(std::size_t index) const = 0;
  virtual unsigned long long getUnsignedLongLong(std::size_t index) const = 0;
  virtual std::tm getDate(std::size_t index) const = 0;
};

class Rowset {
 public:
  virtual ~Rowset() = default;
  virtual void reset() = 0;
  virtual bool is_done() = 0;
  virtual Row& getCurrent() = 0;
  virtual void next() = 0;
};

class ConnectionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};
// Indicates that the error might be caused by a malformed
// query, constraint violation or something else that won't
// fix itself on a retry.
class StatementError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class Statement {
 public:
  explicit Statement(const std::string &query)
    : query_(query) {
  }

  virtual ~Statement() = default;
  virtual std::unique_ptr<Rowset> execute(const std::vector<std::string> &args = {}) = 0;

 protected:
  std::string query_;
};

class Session {
 public:
  virtual ~Session() = default;
  virtual void begin() = 0;
  virtual void commit() = 0;
  virtual void rollback() = 0;
  virtual void execute(const std::string &statement) = 0;
};

class Connection {
 public:
  virtual ~Connection() = default;
  virtual bool connected(std::string& exception) const = 0;
  virtual std::unique_ptr<Statement> prepareStatement(const std::string &query) const = 0;
  virtual std::unique_ptr<Session> getSession() const = 0;
};

}  // namespace org::apache::nifi::minifi::sql

