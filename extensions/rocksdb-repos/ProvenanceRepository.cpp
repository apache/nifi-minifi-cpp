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

#include "ProvenanceRepository.h"

#include <string>

#include "core/Resource.h"

namespace org::apache::nifi::minifi::provenance {

void ProvenanceRepository::printStats() {
  std::string key_count;
  db_->GetProperty("rocksdb.estimate-num-keys", &key_count);

  std::string table_readers;
  db_->GetProperty("rocksdb.estimate-table-readers-mem", &table_readers);

  std::string all_memtables;
  db_->GetProperty("rocksdb.cur-size-all-mem-tables", &all_memtables);

  logger_->log_info("Repository stats: key count: %s, table readers size: %s, all memory tables size: %s",
                    key_count, table_readers, all_memtables);
}

void ProvenanceRepository::run() {
  size_t count = 0;
  while (isRunning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    count++;
    // Hack, to be removed in scope of https://issues.apache.org/jira/browse/MINIFICPP-1145
    count = count % 30;
    if (count == 0) {
      printStats();
    }
  }
}

bool ProvenanceRepository::initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) {
  std::string value;
  if (config->get(Configure::nifi_provenance_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  logger_->log_debug("MiNiFi Provenance Repository Directory %s", directory_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
    core::Property::StringToInt(value, max_partition_bytes_);
  }
  logger_->log_debug("MiNiFi Provenance Max Partition Bytes %d", max_partition_bytes_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
    if (auto max_partition = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value))
      max_partition_millis_ = *max_partition;
  }
  logger_->log_debug("MiNiFi Provenance Max Storage Time: [%" PRId64 "] ms",
                      int64_t{max_partition_millis_.count()});
  rocksdb::Options options;
  options.create_if_missing = true;
  options.use_direct_io_for_flush_and_compaction = true;
  options.use_direct_reads = true;
  // Rocksdb write buffers act as a log of database operation: grow till reaching the limit, serialized after
  // This shouldn't go above 16MB and the configured total size of the db should cap it as well
  int64_t max_buffer_size = 16 << 20;
  options.write_buffer_size = gsl::narrow<size_t>(std::min(max_buffer_size, max_partition_bytes_));
  options.max_write_buffer_number = 4;
  options.min_write_buffer_number_to_merge = 1;

  options.compaction_style = rocksdb::CompactionStyle::kCompactionStyleFIFO;
  options.compaction_options_fifo = rocksdb::CompactionOptionsFIFO(max_partition_bytes_, false);
  if (max_partition_millis_ > std::chrono::milliseconds(0)) {
    options.ttl = std::chrono::duration_cast<std::chrono::seconds>(max_partition_millis_).count();
  }

  logger_->log_info("Write buffer: %llu", options.write_buffer_size);
  logger_->log_info("Max partition bytes: %llu", max_partition_bytes_);
  logger_->log_info("Ttl: %llu", options.ttl);

  rocksdb::DB* db;
  rocksdb::Status status = rocksdb::DB::Open(options, directory_, &db);
  if (status.ok()) {
    logger_->log_debug("MiNiFi Provenance Repository database open %s success", directory_);
    db_.reset(db);
  } else {
    logger_->log_error("MiNiFi Provenance Repository database open %s failed: %s", directory_, status.ToString());
    return false;
  }

  return true;
}

bool ProvenanceRepository::Put(const std::string& key, const uint8_t *buf, size_t bufLen) {
  // persist to the DB
  rocksdb::Slice value((const char *) buf, bufLen);
  return db_->Put(rocksdb::WriteOptions(), key, value).ok();
}

bool ProvenanceRepository::MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
  rocksdb::WriteBatch batch;
  for (const auto &item : data) {
    const auto buf = item.second->getBuffer().as_span<const char>();
    rocksdb::Slice value(buf.data(), buf.size());
    if (!batch.Put(item.first, value).ok()) {
      return false;
    }
  }
  return db_->Write(rocksdb::WriteOptions(), &batch).ok();
}

bool ProvenanceRepository::Get(const std::string &key, std::string &value) {
  return db_->Get(rocksdb::ReadOptions(), key, &value).ok();
}

bool ProvenanceRepository::getElements(std::vector<std::shared_ptr<core::SerializableComponent>> &records, size_t &max_size) {
  std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
  size_t requested_batch = max_size;
  max_size = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (max_size >= requested_batch)
      break;
    auto eventRead = std::make_shared<ProvenanceEventRecord>();
    std::string key = it->key().ToString();
    io::BufferStream stream(gsl::make_span(it->value()).as_span<const std::byte>());
    if (eventRead->deserialize(stream)) {
      max_size++;
      records.push_back(eventRead);
    }
  }
  return max_size > 0;
}

void ProvenanceRepository::destroy() {
  db_.reset();
}

uint64_t ProvenanceRepository::getKeyCount() const {
  std::string key_count;
  db_->GetProperty("rocksdb.estimate-num-keys", &key_count);

  return std::stoull(key_count);
}

REGISTER_RESOURCE_AS(ProvenanceRepository, InternalResource, ("ProvenanceRepository", "provenancerepository"));

}  // namespace org::apache::nifi::minifi::provenance
