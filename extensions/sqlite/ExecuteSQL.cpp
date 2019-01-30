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

#include "ExecuteSQL.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ExecuteSQL::ConnectionURL(  // NOLINT
    "Connection URL",
    "The database URL to connect to",
    "");
core::Property ExecuteSQL::SQLStatement(  // NOLINT
    "SQL Statement",
    "The SQL statement to execute",
    "");

core::Relationship ExecuteSQL::Success(  // NOLINT
    "success",
    "After a successful SQL execution, result FlowFiles are sent here");
core::Relationship ExecuteSQL::Original(  // NOLINT
    "original",
    "The original FlowFile is sent here");
core::Relationship ExecuteSQL::Failure(  // NOLINT
    "failure",
    "Failures which will not work if retried");

void ExecuteSQL::initialize() {
  std::set<core::Property> properties;
  properties.insert(ConnectionURL);
  properties.insert(SQLStatement);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Original);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));
}

void ExecuteSQL::onSchedule(core::ProcessContext *context,
                            core::ProcessSessionFactory *sessionFactory) {
  context->getProperty(ConnectionURL.getName(), db_url_);

  if (db_url_.empty()) {
    logger_->log_error("Invalid Connection URL");
  }

  context->getProperty(SQLStatement.getName(), sql_);
}

void ExecuteSQL::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                           const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->get());

  try {
    // Use an existing context, if one is available
    std::shared_ptr<minifi::sqlite::SQLiteConnection> db;

    if (conn_q_.try_dequeue(db)) {
      logger_->log_debug("Using available SQLite connection");
    }

    if (!db) {
      logger_->log_info("Creating new SQLite connection");
      if (db_url_.substr(0, 9) == "sqlite://") {
        db = std::make_shared<minifi::sqlite::SQLiteConnection>(db_url_.substr(9));
      } else {
        std::stringstream err_msg;
        err_msg << "Connection URL '" << db_url_ << "' is unsupported";
        logger_->log_error(err_msg.str().c_str());
        throw std::runtime_error("Connection Error");
      }
    }

    auto dynamic_sql = std::make_shared<std::string>();

    if (flow_file) {
      if (sql_.empty()) {
        // SQL is not defined as a property, so get SQL from the file content
        SQLReadCallback cb(dynamic_sql);
        session->read(flow_file, &cb);
      } else {
        // SQL is defined as a property, so get the property dynamically w/ EL support
        context->getProperty(SQLStatement, *dynamic_sql, flow_file);
      }
    }

    auto stmt = flow_file
                ? db->prepare(*dynamic_sql)
                : db->prepare(sql_);

    if (flow_file) {
      for (int i = 1; i < INT_MAX; i++) {
        std::string val;
        std::stringstream val_key;
        val_key << "sql.args." << i << ".value";

        if (!flow_file->getAttribute(val_key.str(), val)) {
          break;
        }

        stmt.bind_text(i, val);
      }
    }

    stmt.step();

    if (!stmt.is_ok()) {
      logger_->log_error("SQL statement execution failed: %s", db->errormsg());
      session->transfer(flow_file, Failure);
    }

    auto num_cols = stmt.column_count();
    std::vector<std::string> col_names;

    for (int i = 0; i < num_cols; i++) {
      col_names.emplace_back(stmt.column_name(i));
    }

    while (stmt.is_ok() && !stmt.is_done()) {
      auto result_ff = session->create();

      for (int i = 0; i < num_cols; i++) {
        result_ff->addAttribute(col_names[i], stmt.column_text(i));
      }

      session->transfer(result_ff, Success);
      stmt.step();
    }

    if (flow_file) {
      session->transfer(flow_file, Original);
    }

    // Make connection available for use again
    if (conn_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing SQLite connection");
      conn_q_.enqueue(db);
    } else {
      logger_->log_info("Destroying SQLite connection because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
    if (flow_file) {
      session->transfer(flow_file, Failure);
    }
    this->yield();
  } catch (...) {
    logger_->log_error("Caught Exception");
    if (flow_file) {
      session->transfer(flow_file, Failure);
    }
    this->yield();
  }
}

int64_t ExecuteSQL::SQLReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  sql_->resize(stream->getSize());
  auto num_read = static_cast<uint64_t >(stream->readData(reinterpret_cast<uint8_t *>(&(*sql_)[0]),
                                                          static_cast<int>(stream->getSize())));

  if (num_read != stream->getSize()) {
    throw std::runtime_error("SQLReadCallback failed to fully read flow file input stream");
  }

  return num_read;
}
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
