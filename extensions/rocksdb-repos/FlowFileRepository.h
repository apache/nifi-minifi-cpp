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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_FLOWFILEREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_FLOWFILEREPOSITORY_H_

#include "utils/file/FileUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "Connection.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

#ifdef WIN32
#define FLOWFILE_REPOSITORY_DIRECTORY ".\\flowfile_repository"
#define FLOWFILE_CHECKPOINT_DIRECTORY ".\\flowfile_checkpoint"
#else
#define FLOWFILE_REPOSITORY_DIRECTORY "./flowfile_repository"
#define FLOWFILE_CHECKPOINT_DIRECTORY "./flowfile_checkpoint"
#endif
#define MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME (600000) // 10 minute
#define FLOWFILE_REPOSITORY_PURGE_PERIOD (2000) // 2000 msec

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using rocksdb as the primary substrate.
 */
class FlowFileRepository : public core::Repository, public std::enable_shared_from_this<FlowFileRepository> {
 public:
  // Constructor

  FlowFileRepository(std::string name, utils::Identifier uuid)
      : FlowFileRepository(name) {
  }

  FlowFileRepository(const std::string repo_name = "", std::string directory = FLOWFILE_REPOSITORY_DIRECTORY, int64_t maxPartitionMillis = MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                     int64_t maxPartitionBytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, uint64_t purgePeriod = FLOWFILE_REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        Repository(repo_name.length() > 0 ? repo_name : core::getClassName<FlowFileRepository>(), directory, maxPartitionMillis, maxPartitionBytes, purgePeriod),
        content_repo_(nullptr),
        checkpoint_(nullptr),
        logger_(logging::LoggerFactory<FlowFileRepository>::getLogger()) {
    db_ = NULL;
  }

  // Destructor
  ~FlowFileRepository() {
    if (db_)
      delete db_;
  }

  virtual void flush();

  // initialize
  virtual bool initialize(const std::shared_ptr<Configure> &configure) {
    std::string value;

    if (configure->get(Configure::nifi_flowfile_repository_directory_default, value)) {
      directory_ = value;
    }
    logger_->log_debug("NiFi FlowFile Repository Directory %s", directory_);
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_size, value)) {
      Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_debug("NiFi FlowFile Max Partition Bytes %d", max_partition_bytes_);
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_time, value)) {
      TimeUnit unit;
      if (Property::StringToTime(value, max_partition_millis_, unit) && Property::ConvertTimeUnitToMS(max_partition_millis_, unit, max_partition_millis_)) {
      }
    }
    logger_->log_debug("NiFi FlowFile Max Storage Time: [%d] ms", max_partition_millis_);
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;
    rocksdb::Status status = rocksdb::DB::Open(options, directory_, &db_);
    if (status.ok()) {
      logger_->log_debug("NiFi FlowFile Repository database open %s success", directory_);
    } else {
      logger_->log_error("NiFi FlowFile Repository database open %s fail", directory_);
      return false;
    }
    return true;
  }

  virtual void run();

  virtual bool Put(std::string key, const uint8_t *buf, size_t bufLen) {
    // persistent to the DB
    rocksdb::Slice value((const char *) buf, bufLen);
    rocksdb::Status status;
    repo_size_ += bufLen;
    status = db_->Put(rocksdb::WriteOptions(), key, value);
    if (status.ok())
      return true;
    else
      return false;
  }
  /**
   * 
   * Deletes the key
   * @return status of the delete operation
   */
  virtual bool Delete(std::string key) {
    keys_to_delete.enqueue(key);
    return true;
  }
  /**
   * Sets the value from the provided key
   * @return status of the get operation.
   */
  virtual bool Get(const std::string &key, std::string &value) {
    if (db_ == nullptr)
      return false;
    rocksdb::Status status;
    status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (status.ok())
      return true;
    else
      return false;
  }

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo);

  void start() {
    if (this->purge_period_ <= 0) {
      return;
    }
    if (running_) {
      return;
    }
    running_ = true;
    thread_ = std::thread(&FlowFileRepository::run, shared_from_this());
    logger_->log_debug("%s Repository Monitor Thread Start", getName());
  }

 private:

  /**
   * Initialize the repository
   */
  void initialize_repository();

  /**
   * Returns true if a checkpoint is needed at startup
   * @return true if a checkpoint is needed.
   */
  bool need_checkpoint();

  /**
   * Prunes stored flow files.
   */
  void prune_stored_flowfiles();

  moodycamel::ConcurrentQueue<std::string> keys_to_delete;
  std::shared_ptr<core::ContentRepository> content_repo_;
  rocksdb::DB* db_;
  std::unique_ptr<rocksdb::Checkpoint> checkpoint_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_FLOWFILEREPOSITORY_H_ */
