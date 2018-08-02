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
#ifndef LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCEREPOSITORY_H_
#define LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCEREPOSITORY_H_

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "provenance/Provenance.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

#define PROVENANCE_DIRECTORY "./provenance_repository"
#define MAX_PROVENANCE_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_PROVENANCE_ENTRY_LIFE_TIME (60000) // 1 minute
#define PROVENANCE_PURGE_PERIOD (2500) // 2500 msec

class ProvenanceRepository : public core::Repository, public std::enable_shared_from_this<ProvenanceRepository> {
 public:

  ProvenanceRepository(std::string name, utils::Identifier uuid)
      : ProvenanceRepository(name){

  }
  // Constructor
  /*!
   * Create a new provenance repository
   */
  ProvenanceRepository(const std::string repo_name = "", std::string directory = PROVENANCE_DIRECTORY, int64_t maxPartitionMillis = MAX_PROVENANCE_ENTRY_LIFE_TIME, int64_t maxPartitionBytes =
  MAX_PROVENANCE_STORAGE_SIZE,
                       uint64_t purgePeriod = PROVENANCE_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        Repository(repo_name.length() > 0 ? repo_name : core::getClassName<ProvenanceRepository>(), directory, maxPartitionMillis, maxPartitionBytes, purgePeriod),
        logger_(logging::LoggerFactory<ProvenanceRepository>::getLogger()) {
    db_ = NULL;
  }

  // Destructor
  virtual ~ProvenanceRepository() {
    if (db_)
      delete db_;
  }

  virtual void flush();

  void start() {
    if (this->purge_period_ <= 0)
      return;
    if (running_)
      return;
    running_ = true;
    thread_ = std::thread(&ProvenanceRepository::run, shared_from_this());
    logger_->log_debug("%s Repository Monitor Thread Start", name_);
  }

  // initialize
  virtual bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) {
    std::string value;
    if (config->get(Configure::nifi_provenance_repository_directory_default, value)) {
      directory_ = value;
    }
    logger_->log_debug("NiFi Provenance Repository Directory %s", directory_);
    if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
      core::Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_debug("NiFi Provenance Max Partition Bytes %d", max_partition_bytes_);
    if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
      core::TimeUnit unit;
      if (core::Property::StringToTime(value, max_partition_millis_, unit) && core::Property::ConvertTimeUnitToMS(max_partition_millis_, unit, max_partition_millis_)) {
      }
    }
    logger_->log_debug("NiFi Provenance Max Storage Time: [%d] ms", max_partition_millis_);
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;
    rocksdb::Status status = rocksdb::DB::Open(options, directory_, &db_);
    if (status.ok()) {
      logger_->log_debug("NiFi Provenance Repository database open %s success", directory_);
    } else {
      logger_->log_error("NiFi Provenance Repository database open %s fail", directory_);
      return false;
    }

    return true;
  }
  // Put
  virtual bool Put(std::string key, const uint8_t *buf, size_t bufLen) {
    if (repo_full_) {
      return false;
    }

    // persist to the DB
    rocksdb::Slice value((const char *) buf, bufLen);
    rocksdb::Status status;
    status = db_->Put(rocksdb::WriteOptions(), key, value);
    if (status.ok())
      return true;
    else
      return false;
  }
  // Delete
  virtual bool Delete(std::string key) {
    keys_to_delete.enqueue(key);
    return true;
  }
  // Get
  virtual bool Get(const std::string &key, std::string &value) {
    rocksdb::Status status;
    status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (status.ok())
      return true;
    else
      return false;
  }

  // Remove event
  void removeEvent(ProvenanceEventRecord *event) {
    Delete(event->getEventId());
  }

  virtual bool Serialize(const std::string &key, const uint8_t *buffer, const size_t bufferSize) {
    return Put(key, buffer, bufferSize);
  }

  virtual bool get(std::vector<std::shared_ptr<core::CoreComponent>> &store, size_t max_size) {
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();
      if (store.size() >= max_size)
        break;
      if (eventRead->DeSerialize((uint8_t *) it->value().data(), (int) it->value().size())) {
        store.push_back(std::dynamic_pointer_cast<core::CoreComponent>(eventRead));
      }
    }
    delete it;
    return true;
  }

  virtual bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &records, size_t &max_size, std::function<std::shared_ptr<core::SerializableComponent>()> lambda) {
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    size_t requested_batch = max_size;
    max_size = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {

      if (max_size >= requested_batch)
        break;
      std::shared_ptr<core::SerializableComponent> eventRead = lambda();
      std::string key = it->key().ToString();
      if (eventRead->DeSerialize((uint8_t *) it->value().data(), (int) it->value().size())) {
        max_size++;
        records.push_back(eventRead);
      }

    }
    delete it;

    if (max_size > 0) {
      return true;
    } else {
      return false;
    }
  }
  //! get record
  void getProvenanceRecord(std::vector<std::shared_ptr<ProvenanceEventRecord>> &records, int maxSize) {
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();
      if (records.size() >= (uint64_t)maxSize)
        break;
      if (eventRead->DeSerialize((uint8_t *) it->value().data(), (int) it->value().size())) {
        records.push_back(eventRead);
      }
    }
    delete it;
  }

  virtual bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) {
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    max_size = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();

      if (store.at(max_size)->DeSerialize((uint8_t *) it->value().data(), (int) it->value().size())) {
        max_size++;
      }
      if (store.size() >= max_size)
        break;
    }
    delete it;
    if (max_size > 0) {
      return true;
    } else {
      return false;
    }
  }
  //! purge record
  void purgeProvenanceRecord(std::vector<std::shared_ptr<ProvenanceEventRecord>> &records) {
    for (auto record : records) {
      Delete(record->getEventId());
    }
    flush();
  }
  // destroy
  void destroy() {
    if (db_) {
      delete db_;
      db_ = NULL;
    }
  }
  // Run function for the thread
  void run();

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceRepository(const ProvenanceRepository &parent) = delete;
  ProvenanceRepository &operator=(const ProvenanceRepository &parent) = delete;

 private:
  moodycamel::ConcurrentQueue<std::string> keys_to_delete;
  rocksdb::DB* db_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCEREPOSITORY_H_ */

