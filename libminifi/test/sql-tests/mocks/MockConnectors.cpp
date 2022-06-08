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

#include "MockConnectors.h"

#include <fstream>
#include <algorithm>
#include <utility>
#include <string>
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

std::size_t MockRow::size() const {
  return column_names_->size();
}

std::string MockRow::getColumnName(std::size_t index) const {
  return column_names_->at(index);
}

bool MockRow::isNull(std::size_t index) const {
  return column_values_.at(index) == "NULL";
}

DataType MockRow::getDataType(std::size_t index) const {
  return column_types_->at(index);
}

std::string MockRow::getString(std::size_t index) const {
  return column_values_.at(index);
}

double MockRow::getDouble(std::size_t index) const {
  return std::stof(column_values_.at(index));
}

int MockRow::getInteger(std::size_t index) const {
  return std::stoi(column_values_.at(index));
}

long long MockRow::getLongLong(std::size_t index) const {  // NOLINT
  return std::stoll(column_values_.at(index));
}

unsigned long long MockRow::getUnsignedLongLong(std::size_t index) const {  // NOLINT
  return static_cast<unsigned long long>(std::stoll(column_values_.at(index)));  // NOLINT
}

std::tm MockRow::getDate(std::size_t /*index*/) const {
  throw std::runtime_error("date not implemented");
}

std::vector<std::string> MockRow::getValues() const {
  return column_values_;
}

std::string MockRow::getValue(const std::string& col_name) const {
  auto it = std::find(column_names_->begin(),  column_names_->end(), col_name);
  if (it != column_names_->end()) {
    return column_values_.at(it-column_names_->begin());
  }
  throw std::runtime_error("Unknown column name for getting value");
}

DataType MockRow::getDataType(const std::string& col_name) const {
  auto it = std::find(column_names_->begin(),  column_names_->end(), col_name);
  if (it != column_names_->end()) {
    return column_types_->at(it-column_names_->begin());
  }
  throw std::runtime_error("Unknown column name for getting type");
}

void MockRowset::addRow(const std::vector<std::string>& column_values) {
  rows_.emplace_back(&column_names_, &column_types_, column_values);
}

void MockRowset::reset() {
  current_row_ = rows_.begin();
}

bool MockRowset::is_done() {
  return current_row_ == rows_.end();
}

Row& MockRowset::getCurrent() {
  return *current_row_;
}

void MockRowset::next() {
  ++current_row_;
}

std::vector<std::string> MockRowset::getColumnNames() const {
  return column_names_;
}

std::vector<DataType> MockRowset::getColumnTypes() const {
  return column_types_;
}

std::vector<MockRow> MockRowset::getRows() const {
  return rows_;
}

std::size_t MockRowset::getColumnIndex(const std::string& col_name) const {
  auto it = std::find(column_names_.begin(),  column_names_.end(), col_name);
  if (it != column_names_.end()) {
    return it-column_names_.begin();
  }
  throw std::runtime_error("Unknown column name for getting index");
}

void MockRowset::sort(const std::string& order_by_col, bool order_ascending) {
  std::sort(rows_.begin(), rows_.end(), [&](const MockRow& first, const MockRow& second) {
    if (order_ascending) {
      return first.getValue(order_by_col) < second.getValue(order_by_col);
    } else {
      return first.getValue(order_by_col) > second.getValue(order_by_col);
    }
  });
}

std::unique_ptr<MockRowset> MockRowset::select(const std::vector<std::string>& cols, const std::function<bool(const MockRow&)>& condition, const std::string& order_by_col, bool order_ascending) {
  if (!order_by_col.empty()) {
    sort(order_by_col, order_ascending);
  }

  std::unique_ptr<MockRowset> rowset;
  if (cols.empty()) {
    rowset = std::make_unique<MockRowset>(column_names_, column_types_);
  } else {
    std::vector<DataType> col_types;
    for (const auto& col : cols) {
      col_types.push_back(column_types_.at(getColumnIndex(col)));
    }
    rowset = std::make_unique<MockRowset>(cols, col_types);
  }

  std::vector<std::string> used_cols = cols.empty() ? column_names_ : cols;
  for (const auto& row : rows_) {
    if (condition(row)) {
      std::vector<std::string> values;
      for (const auto& col : used_cols) {
        values.push_back(row.getValue(col));
      }
      rowset->addRow(values);
    }
  }

  return rowset;
}

std::unique_ptr<Rowset> MockDB::execute(const std::string& query, const std::vector<std::string>& args) {
  if (minifi::utils::StringUtils::startsWith(query, "create table", false)) {
    createTable(query);
  } else if (minifi::utils::StringUtils::startsWith(query, "insert into", false)) {
    insertInto(query, args);
  } else if (minifi::utils::StringUtils::startsWith(query, "select", false)) {
    return select(query, args);
  } else {
    throw std::runtime_error("Unknown query type");
  }

  return nullptr;
}

void MockDB::createTable(const std::string& query) {
  std::smatch match;
  std::regex expr(R"(create table (\w+)\s*\((.*)\);)", std::regex_constants::icase);
  std::regex_search(query, match, expr);
  std::string table_name = match[1];
  auto columns_with_type = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(match[2], ",");
  std::vector<std::string> col_names;
  std::vector<DataType> col_types;
  for (const auto& col_with_type : columns_with_type) {
    auto splitted = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(col_with_type, " ");
    col_names.push_back(splitted[0]);
    col_types.push_back(stringToDataType(splitted[1]));
  }
  tables_.emplace(table_name, MockRowset{col_names, col_types});
  storeDb();
}

void MockDB::insertInto(const std::string& query, const std::vector<std::string>& args) {
  std::string replaced_query = query;
  for (const auto& arg : args) {
    replaced_query = minifi::utils::StringUtils::replaceOne(replaced_query, "?", arg);
  }

  std::smatch match;
  std::regex expr(R"(insert into (\w+)\s*(\((.*)\))*\s*values\s*\((.+)\))", std::regex_constants::icase);
  std::regex_search(replaced_query, match, expr);
  std::string table_name = match[1];
  std::vector<std::string> values = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(match[4], ",");
  for (auto& value : values) {
    value = minifi::utils::StringUtils::removeFramingCharacters(value, '\'');
  }
  auto insert_col_names = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(match[3], ",");
  if (!insert_col_names.empty()) {
    auto col_names = tables_.at(table_name).getColumnNames();
    std::vector<std::string> row;
    for (const auto& col_name : col_names) {
      auto it = std::find(insert_col_names.begin(),  insert_col_names.end(), col_name);
      if (it != insert_col_names.end()) {
        row.push_back(values.at(it-insert_col_names.begin()));
      } else {
        row.push_back("NULL");
      }
    }
    tables_.at(table_name).addRow(row);
  } else {
    tables_.at(table_name).addRow(values);
  }

  storeDb();
}

std::unique_ptr<Rowset> MockDB::select(const std::string& query, const std::vector<std::string>& args) {
  std::string replaced_query = query;
  for (const auto& arg : args) {
    replaced_query = minifi::utils::StringUtils::replaceOne(replaced_query, "?", arg);
  }

  std::smatch match;
  std::regex expr(R"(select\s+(.+)\s+from\s+(\w+)\s*(where ((.+(?= order by))|.+$))*\s*(order by (.+))*)", std::regex_constants::icase);
  std::regex_search(replaced_query, match, expr);
  auto cols = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(match[1], ",");
  if (cols[0] == "*") {
    cols = {};
  }
  std::string table_name = match[2];
  std::string condition_str = match[4];
  auto condition = parseWhereCondition(condition_str);
  std::string order = match[7];
  std::string order_col;
  bool descending = false;
  if (!order.empty()) {
    auto order_col_and_sort = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(order, " ");
    order_col = order_col_and_sort[0];
    descending = minifi::utils::StringUtils::equalsIgnoreCase(order_col_and_sort[1], "desc");
  }
  return tables_.at(table_name).select(cols, condition, order_col, !descending);
}

std::function<bool(const MockRow&)> MockDB::parseWhereCondition(const std::string& full_condition_str) {
  if (full_condition_str.empty()) {
    return [](const MockRow&){ return true; };
  }

  std::vector<std::string> condition_strings;
  // TODO(adebreceni): let StringUtils::split* functions take either multiple delimiters or specify case sensitivity
  for (auto&& condition : minifi::utils::StringUtils::splitAndTrimRemovingEmpty(full_condition_str, "and")) {
    for (auto&& subcondition : minifi::utils::StringUtils::splitAndTrimRemovingEmpty(condition, "AND")) {
      condition_strings.push_back(std::move(subcondition));
    }
  }
  std::vector<std::function<bool(const MockRow&)>> condition_parts;
  for (const auto& condition_str : condition_strings) {
    if (condition_str.find(">") != std::string::npos) {
      auto elements = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(condition_str, ">");
      condition_parts.push_back([elements](const MockRow& row){ return std::stoi(row.getValue(elements[0])) > std::stoi(elements[1]); });
    } else if (condition_str.find("<") != std::string::npos) {
      auto elements = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(condition_str, "<");
      condition_parts.push_back([elements](const MockRow& row){ return std::stoi(row.getValue(elements[0])) < std::stoi(elements[1]); });
    } else if (condition_str.find("=") != std::string::npos) {
      auto elements = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(condition_str, "=");
      condition_parts.push_back([elements](const MockRow& row) {
        if (row.getDataType(elements[0]) == DataType::STRING) {
          return row.getValue(elements[0]) == minifi::utils::StringUtils::removeFramingCharacters(elements[1], '"');
        } else {
          return std::stoi(row.getValue(elements[0])) == std::stoi(elements[1]);
        }
      });
    } else {
      throw std::runtime_error("Unimplemented WHERE condition");
    }
  }

  return [condition_parts](const MockRow& row) {
    bool result = true;
    for (const auto& condition : condition_parts) {
      result = result && condition(row);
    }
    return result;
  };
}

void MockDB::readDb() {
  tables_.clear();
  std::ifstream file(file_path_);
  std::string line;
  ParsePhase phase = ParsePhase::NEW_TABLE;
  std::string table_name;
  std::vector<std::string> column_names;
  std::vector<DataType> column_types;
  while (std::getline(file, line)) {
    switch (phase) {
      case ParsePhase::NEW_TABLE: {
        table_name = minifi::utils::StringUtils::trim(line);
        phase = ParsePhase::COLUMN_NAMES;
        break;
      }
      case ParsePhase::COLUMN_NAMES: {
        column_names = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(line, "|");
        phase = ParsePhase::COLUMN_TYPES;
        break;
      }
      case ParsePhase::COLUMN_TYPES: {
        column_types.clear();
        auto type_strs = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(line, "|");
        for (const auto& type : type_strs) {
          column_types.push_back(stringToDataType(type));
        }
        tables_.emplace(table_name, MockRowset(column_names, column_types));
        phase = ParsePhase::ROW_VALUES;
        break;
      }
      case ParsePhase::ROW_VALUES: {
        line = minifi::utils::StringUtils::trim(line);
        if (line.empty()) {
          phase = ParsePhase::NEW_TABLE;
          break;
        }
        auto cells = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(line, "|");
        tables_.at(table_name).addRow(cells);
        break;
      }
    }
  }
}

void MockDB::storeDb() {
  std::ofstream file(file_path_);
  for (const auto& table : tables_) {
    file << table.first << std::endl;
    auto& rowset = table.second;
    for (const auto& col_name : rowset.getColumnNames()) {
      file << col_name << "|";
    }
    file << std::endl;
    for (const auto& col_type : rowset.getColumnTypes()) {
      file << dataTypeToString(col_type) << "|";
    }
    file << std::endl;
    for (const auto& row : rowset.getRows()) {
      for (const auto& cell : row.getValues()) {
        file << cell << "|";
      }
      file << std::endl;
    }
    file << std::endl;
  }
}

DataType MockDB::stringToDataType(const std::string& type_str) {
  if (utils::StringUtils::equalsIgnoreCase(type_str, "integer")) return DataType::INTEGER;
  if (utils::StringUtils::equalsIgnoreCase(type_str, "text")) return DataType::STRING;
  if (utils::StringUtils::equalsIgnoreCase(type_str, "real")) return DataType::DOUBLE;
  throw std::runtime_error("Unimplemented data type");
}

std::string MockDB::dataTypeToString(DataType data_type) {
  switch (data_type) {
    case DataType::INTEGER: return "integer";
    case DataType::STRING: return "text";
    case DataType::DOUBLE: return "real";
    default: throw std::runtime_error("Unimplemented data type");
  }
}

std::unique_ptr<Rowset> MockStatement::execute(const std::vector<std::string>& args) {
  MockDB db(file_path_);
  return db.execute(query_, args);
}

MockODBCConnection::MockODBCConnection(std::string connectionString)
    : connection_string_(std::move(connectionString)) {
  std::smatch match;
  std::regex expr("Database=(.*)(;|$)");
  std::regex_search(connection_string_, match, expr);
  file_path_ = match[1];
}

bool MockODBCConnection::connected(std::string& /*exception*/) const {
  return true;
}

std::unique_ptr<sql::Statement> MockODBCConnection::prepareStatement(const std::string& query) const {
  return std::make_unique<sql::MockStatement>(query, file_path_);
}

std::unique_ptr<Session> MockODBCConnection::getSession() const {
  return std::make_unique<sql::MockSession>();
}

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
