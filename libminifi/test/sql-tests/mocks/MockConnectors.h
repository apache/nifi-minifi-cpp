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

#include <regex>
#include <map>
#include <vector>
#include <algorithm>
#include <memory>
#include <string>

#include "data/DatabaseConnectors.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::sql {

class MockRow : public Row {
 public:
  MockRow(std::vector<std::string>* column_names, std::vector<DataType>* column_types, const std::vector<std::string>& column_values)
    : column_names_(column_names), column_types_(column_types), column_values_(column_values) {
  }

  std::size_t size() const override;
  std::string getColumnName(std::size_t index) const override;
  bool isNull(std::size_t index) const override;
  DataType getDataType(std::size_t index) const override;
  std::string getString(std::size_t index) const override;
  double getDouble(std::size_t index) const override;
  int getInteger(std::size_t index) const override;
  long long getLongLong(std::size_t index) const override;  // NOLINT Return type comes from SOCI interface
  unsigned long long getUnsignedLongLong(std::size_t index) const override;  // NOLINT Return type comes from SOCI interface
  std::tm getDate(std::size_t /*index*/) const override;

  std::vector<std::string> getValues() const;
  std::string getValue(const std::string& col_name) const;
  DataType getDataType(const std::string& col_name) const;

 private:
  std::vector<std::string>* column_names_;
  std::vector<DataType>* column_types_;
  std::vector<std::string> column_values_;
};

class MockRowset : public Rowset {
 public:
  MockRowset(const std::vector<std::string>& column_names, const std::vector<DataType>& column_types)
    : column_names_(column_names), column_types_(column_types) {
  }

  void addRow(const std::vector<std::string>& column_values);
  void reset() override;
  bool is_done() override;
  Row& getCurrent() override;
  void next() override;

  std::vector<std::string> getColumnNames() const;
  std::vector<DataType> getColumnTypes() const;
  std::vector<MockRow> getRows() const;
  std::size_t getColumnIndex(const std::string& col_name) const;
  void sort(const std::string& order_by_col, bool order_ascending = true);
  std::unique_ptr<MockRowset> select(const std::vector<std::string>& cols, const std::function<bool(const MockRow&)>& condition, const std::string& order_by_col, bool order_ascending = true);

 private:
  std::vector<std::string> column_names_;
  std::vector<DataType> column_types_;
  std::vector<MockRow> rows_;
  std::vector<MockRow>::iterator current_row_;
};

class MockDB {
 public:
  explicit MockDB(const std::string& file_path) : file_path_(file_path) {
    readDb();
  }

  ~MockDB() {
    storeDb();
  }

  std::unique_ptr<Rowset> execute(const std::string& query, const std::vector<std::string>& args);

 private:
  enum class ParsePhase {
    NEW_TABLE,
    COLUMN_NAMES,
    COLUMN_TYPES,
    ROW_VALUES
  };

  void createTable(const std::string& query);
  void insertInto(const std::string& query, const std::vector<std::string>& args);
  std::unique_ptr<Rowset> select(const std::string& query, const std::vector<std::string>& args);
  void readDb();
  void storeDb();

  /**
   * This function parses an and-separated list of conditions in the format of <column_name> [<>=] <value>
   * @param condition_str SQL WHERE condition string with only AND logical operators
   * @return Function object evaluating MockRow according to the condition parameter
   */
  std::function<bool(const MockRow&)> parseWhereCondition(const std::string& condition_str);

  static DataType stringToDataType(const std::string& type_str);
  static std::string dataTypeToString(DataType data_type);

  std::string file_path_;
  std::map<std::string, MockRowset> tables_;
};

class MockStatement : public Statement {
 public:
  explicit MockStatement(const std::string& query, const std::string& file_path)
    : Statement(query), file_path_(file_path) {
  }

  std::unique_ptr<Rowset> execute(const std::vector<std::string>& args = {}) override;

 private:
  std::string file_path_;
};

class MockSession : public Session {
 public:
  void begin() override {
  }

  void commit() override {
  }

  void rollback() override {
  }

  void execute(const std::string& /*statement*/) override {
  }
};

class MockODBCConnection : public Connection {
 public:
  explicit MockODBCConnection(std::string connectionString);
  bool connected(std::string& exception) const override;
  std::unique_ptr<sql::Statement> prepareStatement(const std::string& query) const override;
  std::unique_ptr<Session> getSession() const override;

 private:
  std::string connection_string_;
  std::string file_path_;
};

}  // namespace org::apache::nifi::minifi::sql
