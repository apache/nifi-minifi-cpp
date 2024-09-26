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

#include "SociConnectors.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sql {

void SociRow::setIterator(const soci::rowset<soci::row>::iterator& iter) {
  current_ = iter;
}

soci::rowset<soci::row>::iterator SociRow::getIterator() const {
  return current_;
}

void SociRow::next() {
  ++current_;
}

std::size_t SociRow::size() const {
  return current_->size();
}

std::string SociRow::getColumnName(std::size_t index) const {
  return current_->get_properties(index).get_name();
}

bool SociRow::isNull(std::size_t index) const {
  return current_->get_indicator(index) == soci::i_null;
}

DataType SociRow::getDataType(std::size_t index) const {
  switch (const auto dataType = current_->get_properties(index).get_data_type()) {
    case soci::data_type::dt_string: {
      return DataType::STRING;
    }
    case soci::data_type::dt_double: {
      return DataType::DOUBLE;
    }
    case soci::data_type::dt_integer: {
      return DataType::INTEGER;
    }
    case soci::data_type::dt_long_long: {
      return DataType::LONG_LONG;
    }
    case soci::data_type::dt_unsigned_long_long: {
      return DataType::UNSIGNED_LONG_LONG;
    }
    case soci::data_type::dt_date: {
      return DataType::DATE;
    }
    default: {
      throw minifi::Exception(PROCESSOR_EXCEPTION, "SQLRowsetProcessor: Unsupported data type " + std::to_string(dataType));
    }
  }
}

std::string SociRow::getString(std::size_t index) const {
  return current_->get<std::string>(index);
}

double SociRow::getDouble(std::size_t index) const {
  return current_->get<double>(index);
}

int SociRow::getInteger(std::size_t index) const {
  return current_->get<int>(index);
}

long long SociRow::getLongLong(std::size_t index) const {  // NOLINT(google-runtime-int)
  return current_->get<long long>(index);  // NOLINT(google-runtime-int)
}

unsigned long long SociRow::getUnsignedLongLong(std::size_t index) const {  // NOLINT(google-runtime-int)
  return current_->get<unsigned long long>(index);  // NOLINT(google-runtime-int)
}

std::tm SociRow::getDate(std::size_t index) const {
  return current_->get<std::tm>(index);
}

void SociRowset::reset() {
  current_row_.setIterator(rowset_.begin());
}

bool SociRowset::is_done() {
  return current_row_.getIterator() == rowset_.end();
}

Row& SociRowset::getCurrent() {
  return current_row_;
}

void SociRowset::next() {
  current_row_.next();
}

SociStatement::SociStatement(soci::session &session, const std::string &query)
    : Statement(query), session_(session), logger_(core::logging::LoggerFactory<SociStatement>::getLogger()) {}

std::unique_ptr<Rowset> SociStatement::execute(const std::vector<std::string>& args) {
  try {
    auto stmt = session_.prepare << query_;
    for (auto& arg : args) {
      // binds arguments to the prepared statement
      stmt.operator,(soci::use(arg));
    }
    return std::make_unique<SociRowset>(stmt);
  } catch (const soci::soci_error& ex) {
    logger_->log_error("Error while evaluating query, type: {}, what: {}", typeid(ex).name(), ex.what());
    if (ex.get_error_category() == soci::soci_error::error_category::connection_error
        || ex.get_error_category() == soci::soci_error::error_category::system_error) {
      throw sql::ConnectionError(ex.get_error_message());
    } else {
      throw sql::StatementError(ex.get_error_message());
    }
  } catch (const std::exception& ex) {
    logger_->log_error("Error while evaluating query, type: {}, what: {}", typeid(ex).name(), ex.what());
    throw sql::StatementError(ex.what());
  }
}

void SociSession::begin() {
  session_.begin();
}

void SociSession::commit() {
  session_.commit();
}

void SociSession::rollback() {
  session_.rollback();
}

void SociSession::execute(const std::string &statement) {
  session_ << statement;
}

ODBCConnection::ODBCConnection(std::string connectionString)
  : connection_string_(std::move(connectionString)) {
    session_ = std::make_unique<soci::session>(getSessionParameters());
}

bool ODBCConnection::connected(std::string& exception) const {
  try {
    exception.clear();
    // According to https://stackoverflow.com/questions/3668506/efficient-sql-test-query-or-validation-query-that-will-work-across-all-or-most by Rob Hruska,
    // 'select 1' works for: H2, MySQL, Microsoft SQL Server, PostgreSQL, SQLite. For Oracle 'SELECT 1 FROM DUAL' works.
    prepareStatement("select 1")->execute();
    return true;
  } catch (const std::exception& e) {
    exception = e.what();
    return false;
  }
}

std::unique_ptr<sql::Statement> ODBCConnection::prepareStatement(const std::string& query) const {
  return std::make_unique<sql::SociStatement>(*session_, query);
}

std::unique_ptr<Session> ODBCConnection::getSession() const {
  return std::make_unique<sql::SociSession>(*session_);
}

soci::connection_parameters ODBCConnection::getSessionParameters() const {
  static const soci::backend_factory &backEnd = *soci::factory_odbc();

  soci::connection_parameters parameters(backEnd, connection_string_);
  parameters.set_option(soci::odbc_option_driver_complete, "0" /* SQL_DRIVER_NOPROMPT */);

  return parameters;
}

}  // namespace org::apache::nifi::minifi::sql
