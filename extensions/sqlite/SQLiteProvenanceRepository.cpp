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

#include "SQLiteProvenanceRepository.h"

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Resource.h"
#include "core/logging/LoggerFactory.h"
#include "io/BufferStream.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/ParsingUtils.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::provenance {

namespace {
// RAII deleter for SQLite statements
struct StmtDeleter {
  void operator()(sqlite3_stmt* stmt) const { sqlite3_finalize(stmt); }
};
using unique_stmt = std::unique_ptr<sqlite3_stmt, StmtDeleter>;
}  // namespace

void SQLiteProvenanceRepository::DbDeleter::operator()(sqlite3* db) const {
  sqlite3_close_v2(db);
}

class SQLiteProvenanceRepository::EventCursor : public Cursor {
  friend class SQLiteProvenanceRepository;
 public:
  explicit EventCursor(uint64_t event_id, std::string key): event_id_(event_id), key_(std::move(key)) {}
  explicit EventCursor(std::string_view cursor_str) {
    io::BufferStream stream{utils::string::from_hex(cursor_str)};
    stream.read(event_id_);
    stream.read(key_, io::LengthPrefixSize::_16BIT, std::numeric_limits<uint16_t>::max());
  }
  [[nodiscard]]
  std::string toString() const override {
    io::BufferStream stream;
    stream.write(event_id_);
    stream.write(key_);
    return utils::string::to_hex(stream.getBuffer());
  }
  ~EventCursor() override = default;

 private:
  uint64_t event_id_;
  std::string key_;
};

SQLiteProvenanceRepository::SQLiteProvenanceRepository(std::string_view repo_name,
    std::string directory,
    std::chrono::milliseconds maxPartitionMillis,
    int64_t maxPartitionBytes,
    std::chrono::milliseconds purgePeriod)
  : ThreadedRepositoryImpl(repo_name.length() > 0 ? repo_name : core::className<SQLiteProvenanceRepository>(),
      std::move(directory), maxPartitionMillis, maxPartitionBytes, purgePeriod),
    logger_(core::logging::LoggerFactory<SQLiteProvenanceRepository>::getLogger()) {
}

SQLiteProvenanceRepository::~SQLiteProvenanceRepository() {
  stop();
}

bool SQLiteProvenanceRepository::initialize(const std::shared_ptr<Configure>& config) {
  std::string value;
  if (config->get(Configure::nifi_provenance_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
    utils::file::create_dir(directory_, true);
  }
  logger_->log_debug("Provenance Repository Path {}", directory_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
    max_partition_bytes_ = gsl::narrow<int64_t>(parsing::parseDataSize(value) | utils::orThrow("expected parsable data size"));
  }
  logger_->log_debug("Provenance Max Size {}", max_partition_bytes_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
    if (auto max_partition = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value)) {
      max_partition_millis_ = *max_partition;
    }
  }

  sqlite3* db_ptr = nullptr;

  // Open with SQLITE_OPEN_FULLMUTEX to ensure thread-safe connection sharing
  // between the append calls and the background cleanup thread.
  int rc = sqlite3_open_v2((std::filesystem::path(directory_) / "provenance.db").string().c_str(), &db_ptr, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
  db_.reset(db_ptr);

  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_.get())));
  }

  // Crucial: Set a busy timeout so the append and cleanup threads
  // gracefully wait for each other instead of immediately throwing SQLITE_BUSY.
  sqlite3_busy_timeout(db_.get(), 5000);

  // Enable foreign keys for cascading deletes and WAL mode for better concurrency
  sqlite3_exec(db_.get(), "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

  initializeSchema();

  return true;
}

void SQLiteProvenanceRepository::run() {
  while (isRunning()) {
    std::this_thread::sleep_for(purge_period_);
    purge();
  }
}

std::expected<void, std::string> SQLiteProvenanceRepository::appendEvents(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) {
  if (events.empty()) {
    return {};
  }

  std::vector<std::pair<std::unique_ptr<io::BufferStream>, std::vector<std::string>>> event_data;
  event_data.reserve(events.size());
  for (auto& event : events) {
    event_data.emplace_back(std::make_unique<io::BufferStream>(), std::vector<std::string>{event->getUUIDStr(), event->getFlowFileUuid().to_string()});
    for (auto& id : event->getLineageIdentifiers()) {
      event_data.back().second.push_back(id.to_string());
    }

    for (auto& id : event->getParentUuids()) {
      event_data.back().second.push_back(id.to_string());
    }

    for (auto& id : event->getChildrenUuids()) {
      event_data.back().second.push_back(id.to_string());
    }
    event->serialize(*event_data.back().first);
  }

  char* err_msg = nullptr;
  sqlite3_exec(db_.get(), "BEGIN TRANSACTION", nullptr, nullptr, &err_msg);

  // 1. Prepare statements ONCE for the entire batch
  sqlite3_stmt* stmt_event_ptr;
  if (sqlite3_prepare_v2(db_.get(), "INSERT INTO events (data) VALUES (?)", -1, &stmt_event_ptr, nullptr) != SQLITE_OK) {
    sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
    return std::unexpected(sqlite3_errmsg(db_.get()));
  }
  unique_stmt stmt_event(stmt_event_ptr);

  sqlite3_stmt* stmt_key_ptr;
  if (sqlite3_prepare_v2(db_.get(), "INSERT INTO event_keys (event_id, key_data) VALUES (?, ?)", -1, &stmt_key_ptr, nullptr) != SQLITE_OK) {
    sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
    return std::unexpected(sqlite3_errmsg(db_.get()));
  }
  unique_stmt stmt_key(stmt_key_ptr);

  std::vector<uint64_t> event_ordinals;
  event_ordinals.reserve(event_data.size());

  // 2. Loop through all events and reuse the prepared statements
  for (auto& event : event_data) {
    sqlite3_bind_blob(stmt_event.get(), 1, event.first->getBuffer().data(), event.first->getBuffer().size(), SQLITE_STATIC);

    if (sqlite3_step(stmt_event.get()) != SQLITE_DONE) {
      sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
      return std::unexpected(sqlite3_errmsg(db_.get()));
    }

    int64_t event_ordinal = sqlite3_last_insert_rowid(db_.get());
    event_ordinals.push_back(gsl::narrow<uint64_t>(event_ordinal));

    // Reset the statement so it can be used for the next event
    sqlite3_reset(stmt_event.get());

    for (const auto& key : event.second) {
      sqlite3_bind_int64(stmt_key.get(), 1, event_ordinal);
      sqlite3_bind_blob(stmt_key.get(), 2, key.data(), key.length(), SQLITE_STATIC);

      if (sqlite3_step(stmt_key.get()) != SQLITE_DONE) {
        sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        return std::unexpected(sqlite3_errmsg(db_.get()));
      }

      // Reset the statement so it can be used for the next key
      sqlite3_reset(stmt_key.get());
    }
  }

  sqlite3_exec(db_.get(), "COMMIT", nullptr, nullptr, nullptr);

  // only assigned once the whole batch is stored, a rolled back batch leaves the events untouched
  for (size_t i = 0; i < events.size(); ++i) {
    events[i]->setEventOrdinal(event_ordinals[i]);
  }

  return {};
}

std::unique_ptr<ProvenanceRepository::Cursor> SQLiteProvenanceRepository::cursorFromString(std::string_view cursor_str) {
  if (cursor_str.empty()) {
    return std::make_unique<EventCursor>(0, "");
  }
  return std::make_unique<EventCursor>(cursor_str);
}

std::expected<std::vector<std::shared_ptr<ProvenanceEventRecord>>, std::string> SQLiteProvenanceRepository::getEvents(size_t max_size, Cursor* cursor) {
  auto* event_cursor = dynamic_cast<EventCursor*>(cursor);
  if (!event_cursor) {
    return std::unexpected{"Invalid cursor"};
  }
  std::string sql = "SELECT e.id, e.data FROM events e ";

  if (!event_cursor->key_.empty()) {
    // an event can be related to the same key in multiple ways (e.g. as lineage and as parent), so a
    // join would return such events multiple times
    sql += "WHERE e.id IN (SELECT event_id FROM event_keys WHERE key_data = ?) AND e.id > ? ";
  } else {
    sql += "WHERE e.id > ? ";
  }

  sql += "ORDER BY e.id ASC;";

  sqlite3_stmt* stmt_ptr;
  if (sqlite3_prepare_v2(db_.get(), sql.c_str(), -1, &stmt_ptr, nullptr) != SQLITE_OK) {
    return std::unexpected(sqlite3_errmsg(db_.get()));
  }
  unique_stmt stmt(stmt_ptr);

  int bind_idx = 1;
  if (!event_cursor->key_.empty()) {
    sqlite3_bind_blob(stmt.get(), bind_idx++, event_cursor->key_.data(), event_cursor->key_.size(), SQLITE_STATIC);
  }
  sqlite3_bind_int64(stmt.get(), bind_idx++, event_cursor->event_id_);

  std::vector<std::shared_ptr<ProvenanceEventRecord>> events;

  uint64_t last_event_id = event_cursor->event_id_;

  while (events.size() < max_size && sqlite3_step(stmt.get()) == SQLITE_ROW) {
    last_event_id = gsl::narrow<uint64_t>(sqlite3_column_int64(stmt.get(), 0));

    const void* blob_data = sqlite3_column_blob(stmt.get(), 1);
    int blob_bytes = sqlite3_column_bytes(stmt.get(), 1);

    auto event = ProvenanceEventRecord::create();
    io::BufferStream stream(std::span{static_cast<const std::byte*>(blob_data), gsl::narrow<size_t>(blob_bytes)});
    if (event->deserialize(stream)) {
      event->setEventOrdinal(last_event_id);
      events.push_back(std::move(event));
    }
  }

  event_cursor->event_id_ = last_event_id;

  return events;
}

uint64_t SQLiteProvenanceRepository::getRepositorySize() const {
  int64_t page_count = 0;
  int64_t freelist_count = 0;
  int64_t page_size = 0;

  auto execute_pragma = [&](const char* query, int64_t& out_val) {
    sqlite3_stmt* stmt_ptr = nullptr;
    if (sqlite3_prepare_v2(db_.get(), query, -1, &stmt_ptr, nullptr) == SQLITE_OK) {
      unique_stmt stmt(stmt_ptr);
      if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        out_val = sqlite3_column_int64(stmt.get(), 0);
      }
    }
  };

  execute_pragma("PRAGMA page_count;", page_count);
  execute_pragma("PRAGMA freelist_count;", freelist_count);
  execute_pragma("PRAGMA page_size;", page_size);

  // Active size = (Total Pages - Free Pages) * Page Size
  return gsl::narrow<uint64_t>((page_count - freelist_count) * page_size);
}

uint64_t SQLiteProvenanceRepository::getRepositoryEntryCount() const {
  if (!db_) {
    return 0;
  }

  const char* sql = "SELECT COUNT(*) FROM events;";
  sqlite3_stmt* stmt_ptr = nullptr;

  if (sqlite3_prepare_v2(db_.get(), sql, -1, &stmt_ptr, nullptr) != SQLITE_OK) {
    logger_->log_error("Failed to prepare entry count statement: {}", sqlite3_errmsg(db_.get()));
    return 0;
  }

  unique_stmt stmt(stmt_ptr);
  uint64_t count = 0;

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    count = gsl::narrow<uint64_t>(sqlite3_column_int64(stmt.get(), 0));
  } else {
    logger_->log_error("Failed to execute entry count statement: {}", sqlite3_errmsg(db_.get()));
  }

  return count;
}

void SQLiteProvenanceRepository::initializeSchema() {
  // Store timestamp as Unix epoch ms for easier TTL calculations
  const char* schema = R"(
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER DEFAULT (CAST((julianday('now') - 2440587.5)*86400000 AS INTEGER)),
            data BLOB
        );
        CREATE TABLE IF NOT EXISTS event_keys (
            event_id INTEGER,
            key_data BLOB,
            FOREIGN KEY(event_id) REFERENCES events(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_event_keys_key ON event_keys(key_data);
        CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);
    )";
  char* err_msg = nullptr;
  if (sqlite3_exec(db_.get(), schema, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string err(err_msg);
    sqlite3_free(err_msg);
    throw std::runtime_error("Failed to initialize schema: " + err);
  }
}

void SQLiteProvenanceRepository::purge() {
  if (!db_) return;

  // 1. Time-based eviction
  if (max_partition_millis_.count() > 0) {
    const char* time_sql = "DELETE FROM events WHERE timestamp < (CAST((julianday('now') - 2440587.5)*86400000 AS INTEGER) - ?);";
    sqlite3_stmt* stmt_time_ptr = nullptr;
    if (sqlite3_prepare_v2(db_.get(), time_sql, -1, &stmt_time_ptr, nullptr) == SQLITE_OK) {
      unique_stmt stmt_time(stmt_time_ptr);
      sqlite3_bind_int64(stmt_time.get(), 1, max_partition_millis_.count());
      sqlite3_step(stmt_time.get());
    }
  }

  // 2. Size-based eviction
  if (max_partition_bytes_ > 0) {
    while (getRepositorySize() > gsl::narrow<uint64_t>(max_partition_bytes_)) {
      // Delete older events in batches of 1000 to keep the lock window short
      const char* size_sql = "DELETE FROM events WHERE id IN (SELECT id FROM events ORDER BY timestamp ASC LIMIT 1000);";
      char* err_msg = nullptr;
      if (sqlite3_exec(db_.get(), size_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        if (err_msg) {
          logger_->log_error("Size-based purge failed: {}", err_msg);
          sqlite3_free(err_msg);
        }
        break;  // Avoid infinite looping if the delete query fails
      }
    }
  }
}

REGISTER_RESOURCE_AS(SQLiteProvenanceRepository, InternalResource, ("SQLiteProvenanceRepository", "sqliteprovenancerepository"));

}  // namespace org::apache::nifi::minifi::provenance
