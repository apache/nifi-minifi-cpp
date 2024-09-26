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

bool ProvenanceRepository::initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) {
  std::string value;
  if (config->get(Configure::nifi_provenance_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  logger_->log_debug("MiNiFi Provenance Repository Directory {}", directory_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
    core::Property::StringToInt(value, max_partition_bytes_);
  }
  logger_->log_debug("MiNiFi Provenance Max Partition Bytes {}", max_partition_bytes_);
  if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
    if (auto max_partition = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value))
      max_partition_millis_ = *max_partition;
  }
  logger_->log_debug("MiNiFi Provenance Max Storage Time: [{}]", max_partition_millis_);

  auto db_options = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    minifi::internal::setCommonRocksDbOptions(db_opts);
  };

  // Rocksdb write buffers act as a log of database operation: grow till reaching the limit, serialized after
  // This shouldn't go above 16MB and the configured total size of the db should cap it as well
  auto cf_options = [this] (rocksdb::ColumnFamilyOptions& cf_opts) {
    int64_t max_buffer_size = 16 << 20;
    cf_opts.write_buffer_size = gsl::narrow<size_t>(std::min(max_buffer_size, max_partition_bytes_));
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
  if (db_->open()) {
    logger_->log_debug("MiNiFi Provenance Repository database open {} success", directory_);
  } else {
    logger_->log_error("MiNiFi Provenance Repository database open {} failed", directory_);
    return false;
  }

  return true;
}

bool ProvenanceRepository::getElements(std::vector<std::shared_ptr<core::SerializableComponent>> &records, size_t &max_size) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  std::unique_ptr<rocksdb::Iterator> it(opendb->NewIterator(rocksdb::ReadOptions()));
  size_t requested_batch = max_size;
  max_size = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (max_size >= requested_batch)
      break;
    auto eventRead = ProvenanceEventRecord::create();
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

REGISTER_RESOURCE_AS(ProvenanceRepository, InternalResource, ("ProvenanceRepository", "provenancerepository"));

}  // namespace org::apache::nifi::minifi::provenance
