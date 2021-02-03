/**
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

#include <sqlite3.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sqlite {

class SQLiteConnection;

/**
 * RAII wrapper for a sqlite3 prepared statement
 */
class SQLiteStatement {
 public:
  SQLiteStatement(sqlite3 *db, const std::string &sql)
      : logger_(logging::LoggerFactory<SQLiteConnection>::getLogger()) {
    if (sqlite3_prepare_v3(db, sql.c_str(), sql.size(), 0, &stmt_, nullptr)) {
      std::stringstream err_msg;
      err_msg << "Failed to create prepared statement: ";
      err_msg << sql;
      err_msg << " because ";
      err_msg << sqlite3_errmsg(db);
      throw std::runtime_error(err_msg.str());
    }

    if (!stmt_) {
      std::stringstream err_msg;
      err_msg << "Failed to create prepared statement: ";
      err_msg << sql;
      err_msg << " because statement was NULL";
      throw std::runtime_error(err_msg.str());
    }

    db_ = db;
  }

  ~SQLiteStatement() {
    sqlite3_finalize(stmt_);
  }

  void bind_text(int pos, const std::string &text) {
    if (sqlite3_bind_text(stmt_, pos, text.c_str(), text.size(), SQLITE_TRANSIENT)) {
      std::stringstream err_msg;
      err_msg << "Failed to bind text parameter"
              << pos
              << ": "
              << text
              << " because "
              << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  void bind_int64(int pos, int64_t val) {
    if (sqlite3_bind_int64(stmt_, pos, val)) {
      std::stringstream err_msg;
      err_msg << "Failed to bind int64 parameter"
              << pos
              << ": "
              << val
              << " because "
              << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  void bind_double(int pos, double val) {
    if (sqlite3_bind_double(stmt_, pos, val)) {
      std::stringstream err_msg;
      err_msg << "Failed to bind double parameter"
              << pos
              << ": "
              << val
              << " because "
              << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  void bind_null(int pos) {
    if (sqlite3_bind_null(stmt_, pos)) {
      std::stringstream err_msg;
      err_msg << "Failed to bind NULL parameter"
              << pos
              << " because "
              << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  void step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_BUSY) {
      reset_flags();
      is_ok_ = false;
      is_busy_ = true;
    } else if (rc == SQLITE_DONE) {
      reset_flags();
      is_done_ = true;
    } else if (rc == SQLITE_ROW) {
      reset_flags();
      is_row_ = true;
    } else {
      is_ok_ = false;
      is_error_ = true;
      std::stringstream err_msg;
      err_msg << "Failed to step statement because "
              << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  bool is_ok() {
    return is_ok_;
  }

  bool is_done() {
    return is_done_;
  }

  bool is_row() {
    return is_row_;
  }

  bool is_error() {
    return is_error_;
  }

  bool is_busy() {
    return is_busy_;
  }

  std::string column_text(int col) {
    return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt_, col)));
  }

  int64_t  column_int64(int col) {
    return sqlite3_column_int64(stmt_, col);
  }

  double column_double(int col) {
    return sqlite3_column_double(stmt_, col);
  }

  bool column_is_int(int col) {
    return SQLITE_INTEGER == sqlite3_column_type(stmt_, col);
  }

  bool column_is_float(int col) {
    return SQLITE_FLOAT == sqlite3_column_type(stmt_, col);
  }

  bool column_is_text(int col) {
    return SQLITE_TEXT == sqlite3_column_type(stmt_, col);
  }

  bool column_is_blob(int col) {
    return SQLITE_BLOB == sqlite3_column_type(stmt_, col);
  }

  bool column_is_null(int col) {
    return SQLITE_NULL == sqlite3_column_type(stmt_, col);
  }

  std::string column_name(int col) {
    return std::string(sqlite3_column_name(stmt_, col));
  }

  int column_count() {
    return sqlite3_column_count(stmt_);
  }

  void reset() {
    sqlite3_reset(stmt_);
  }

 private:
  std::shared_ptr<logging::Logger> logger_;

  sqlite3_stmt *stmt_;
  sqlite3 *db_ = nullptr;
  bool is_ok_ = true;
  bool is_busy_ = false;
  bool is_done_ = false;
  bool is_error_ = false;
  bool is_row_ = false;

  void reset_flags() {
    is_ok_ = true;
    is_busy_ = false;
    is_done_ = false;
    is_error_ = false;
    is_row_ = false;
  }
};

/**
 * RAII wrapper for a sqlite3 connection
 */
class SQLiteConnection {
 public:
  SQLiteConnection(const std::string &filename)
      : logger_(logging::LoggerFactory<SQLiteConnection>::getLogger()),
        filename_(filename) {
    logger_->log_info("Opening SQLite database: %s", filename_);

    if (sqlite3_open(filename_.c_str(), &db_)) {
      std::stringstream err_msg("Failed to open database: ");
      err_msg << filename_;
      err_msg << " because ";
      err_msg << sqlite3_errmsg(db_);
      throw std::runtime_error(err_msg.str());
    }
  }

  SQLiteConnection(SQLiteConnection &&other)
      : logger_(std::move(other.logger_)),
        filename_(std::move(other.filename_)),
        db_(other.db_) {
    other.db_ = nullptr;
  }

  ~SQLiteConnection() {
    logger_->log_info("Closing SQLite database: %s", filename_);
    sqlite3_close(db_);
  }

  SQLiteStatement prepare(const std::string sql) {
    return SQLiteStatement(db_, sql);
  }

  std::string errormsg() {
    return sqlite3_errmsg(db_);
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string filename_;

  sqlite3 *db_ = nullptr;
};

}  // namespace sqlite
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
