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

#include "RocksDbProvenanceRepository.h"

#include <string>

#include "core/Resource.h"
#include "minifi-cpp/utils/Literals.h"

namespace org::apache::nifi::minifi::provenance {

namespace {
class EventCursor : public ProvenanceRepository::Cursor {
public:
  explicit EventCursor(std::string event_key): event_key_(std::move(event_key)) {}
  EventCursor(const EventCursor&) = default;
  EventCursor(EventCursor&&) = default;
  EventCursor& operator=(const EventCursor&) = default;
  EventCursor& operator=(EventCursor&&) = default;

  [[nodiscard]]
  std::string toString() const override {
    return event_key_;
  }
  ~EventCursor() override = default;

  std::string event_key_;
};
}  // namespace

static const std::string_view NEXT_EVENT_KEY = "next_event_key";

bool RocksDbProvenanceRepository::initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) {
  if (!RocksDbRepository::initialize(config)) {
    return false;
  }
  std::string value;
  if (config->get(Configure::nifi_provenance_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  logger_->log_debug("MiNiFi Provenance Repository Directory {}", directory_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
    max_partition_bytes_ = gsl::narrow<int64_t>(parsing::parseDataSize(value) | utils::orThrow("expected parsable data size"));
  }
  logger_->log_debug("MiNiFi Provenance Max Partition Bytes {}", max_partition_bytes_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
    if (auto max_partition = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value)) {
      max_partition_millis_ = *max_partition;
    }
  }
  logger_->log_debug("MiNiFi Provenance Max Storage Time: [{}]", max_partition_millis_);

  verify_checksums_in_rocksdb_reads_ = (config->get(Configure::nifi_provenance_repository_rocksdb_read_verify_checksums) | utils::andThen(&utils::string::toBool)).value_or(false);
  logger_->log_debug("{} checksum verification in RocksDbProvenanceRepository", verify_checksums_in_rocksdb_reads_ ? "Using" : "Not using");

  auto db_options = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    minifi::internal::setCommonRocksDbOptions(db_opts);
  };

  // Rocksdb write buffers act as a log of database operation: grow till reaching the limit, serialized after
  // This shouldn't go above 16MB and the configured total size of the db should cap it as well
  auto cf_options = [this] (rocksdb::ColumnFamilyOptions& cf_opts) {
    cf_opts.write_buffer_size = std::min<size_t>(16_MiB, max_partition_bytes_);
    cf_opts.max_write_buffer_number = 4;
    cf_opts.min_write_buffer_number_to_merge = 1;

    cf_opts.compaction_style = rocksdb::CompactionStyle::kCompactionStyleFIFO;
    cf_opts.compaction_options_fifo = rocksdb::CompactionOptionsFIFO(max_partition_bytes_, false);
    if (max_partition_millis_ > std::chrono::milliseconds(0)) {
      cf_opts.ttl = std::chrono::duration_cast<std::chrono::seconds>(max_partition_millis_).count();
    }
  };

  db_ = minifi::internal::RocksDatabase::create(db_options, cf_options, directory_,
    minifi::internal::getRocksDbOptionsToOverride(config, Configure::nifi_provenance_repository_rocksdb_options));
  std::string internal_state_db_uri = [&] {
    const std::string_view minifidb_scheme = "minifidb://";
    if (directory_.starts_with(minifidb_scheme)) {
      return directory_ + "-internal-state";
    }
    std::string uri = utils::string::join_pack(minifidb_scheme, directory_);
    if (uri.ends_with("/") || uri.ends_with("\\")) {
      uri.pop_back();
    }
    return uri + "/internal-state";
  }();
  internal_state_db_ = minifi::internal::RocksDatabase::create(db_options, {}, internal_state_db_uri, {});
  if (auto open_state_db = internal_state_db_->open()) {
    rocksdb::ReadOptions options;
    options.verify_checksums = verify_checksums_in_rocksdb_reads_;
    std::string next_event_key_str;
    if (open_state_db->Get(options, NEXT_EVENT_KEY, &next_event_key_str).ok()) {
      next_event_key_ = std::stoull(next_event_key_str);
    } else {
      logger_->log_debug("Could not find '{}', resetting event key", NEXT_EVENT_KEY);
      next_event_key_ = 1;
    }
    logger_->log_trace("Using next event key: {}", next_event_key_);
  } else {
    logger_->log_error("Could not open internal state column in provenance repository {}", internal_state_db_uri);
    return false;
  }
  if (db_->open()) {
    logger_->log_debug("MiNiFi Provenance Repository database open {} success", directory_);
  } else {
    logger_->log_error("MiNiFi Provenance Repository database open {} failed", directory_);
    return false;
  }

  return true;
}

void RocksDbProvenanceRepository::destroy() {
  db_.reset();
}

std::unique_ptr<ProvenanceRepository::Cursor> RocksDbProvenanceRepository::cursorFromString(std::string_view cursor_str) {
  return std::make_unique<EventCursor>(std::string{cursor_str});
}

std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> RocksDbProvenanceRepository::getEvents(size_t max_size, Cursor* cursor) {
  auto* event_cursor = dynamic_cast<EventCursor*>(cursor);
  if (cursor && !event_cursor) {
    return std::unexpected{"Invalid cursor"};
  }
  if (max_size == 0) {
    return {};
  }
  auto opendb = db_->open();
  if (!opendb) {
    return std::unexpected{"Failed to open database"};
  }
  std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> records;
  rocksdb::ReadOptions options;
  options.verify_checksums = verify_checksums_in_rocksdb_reads_;
  std::unique_ptr<rocksdb::Iterator> it(opendb->NewIterator(options));
  std::string last_event_key;
  if (event_cursor) {
    last_event_key = event_cursor->event_key_;
    it->Seek(event_cursor->event_key_);
    if (it->Valid() && it->key() == event_cursor->event_key_) {
      it->Next();
    }
  } else {
    it->SeekToFirst();
  }
  for (; it->Valid(); it->Next()) {
    last_event_key = it->key().ToString();
    auto eventRead = ProvenanceEventRecord::create();
    const auto slice = it->value();
    io::BufferStream stream(std::as_bytes(std::span(slice.data(), slice.size())));
    if (eventRead->deserialize(stream)) {
      records.push_back(eventRead);
      if (--max_size == 0) {
        break;
      }
    }
  }
  if (event_cursor) {
    event_cursor->event_key_ = last_event_key;
  }
  return records;
}

std::expected<void, std::string> RocksDbProvenanceRepository::appendEvents(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) {
  EntryStreams data;
  data.reserve(events.size());
  std::lock_guard guard(next_event_key_mtx_);
  for (auto& event : events) {
    event->setEventOrdinal(next_event_key_++);
  }
  {
    auto open_state_db = internal_state_db_->open();
    if (!open_state_db) {
      return std::unexpected{"Failed to open internal state column in provenance database"};
    }
    auto operation = [this, &open_state_db]() { return open_state_db->Put(rocksdb::WriteOptions(), NEXT_EVENT_KEY, std::to_string(next_event_key_)); };
    if (!ExecuteWithRetry(operation)) {
      return std::unexpected{"Failed to update next provenance event key"};
    }
  }
  for (auto& event : events) {
    data.emplace_back(std::to_string(event->getEventOrdinal()), std::make_unique<io::BufferStream>());
    if (!event->serialize(*data.back().second)) {
      return std::unexpected{fmt::format("Failed to serialize provenance event '{}'", event->getUUIDStr())};
    }
  }
  if (MultiPut(data)) {
    return {};
  }

  return std::unexpected{"Failed to append provenance events"};
}

REGISTER_RESOURCE_AS(RocksDbProvenanceRepository, InternalResource, ("RocksDbProvenanceRepository", "rocksdbprovenancerepository", "ProvenanceRepository", "provenancerepository"));

}  // namespace org::apache::nifi::minifi::provenance
