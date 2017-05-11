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

#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "Connection.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

#define FLOWFILE_REPOSITORY_DIRECTORY "./flowfile_repository"
#define MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME (600000) // 10 minute
#define FLOWFILE_REPOSITORY_PURGE_PERIOD (2500) // 2500 msec

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using LevelDB as the primary substrate.
 */
class FlowFileRepository : public core::Repository,
    public std::enable_shared_from_this<FlowFileRepository> {
 public:
  // Constructor

  FlowFileRepository(const std::string repo_name = "", std::string directory=FLOWFILE_REPOSITORY_DIRECTORY, int64_t maxPartitionMillis=MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                     int64_t maxPartitionBytes=MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, uint64_t purgePeriod=FLOWFILE_REPOSITORY_PURGE_PERIOD)
      : Repository(repo_name.length() > 0 ? repo_name : core::getClassName<FlowFileRepository>(), directory,
                   maxPartitionMillis, maxPartitionBytes, purgePeriod),
        logger_(logging::LoggerFactory<FlowFileRepository>::getLogger())

  {
    db_ = NULL;
  }


  // Destructor
  ~FlowFileRepository() {
    if (db_)
      delete db_;
  }

  // initialize
  virtual bool initialize(const std::shared_ptr<Configure> &configure) {
    std::string value;

    if (configure->get(Configure::nifi_flowfile_repository_directory_default,
                        value)) {
      directory_ = value;
    }
    logger_->log_info("NiFi FlowFile Repository Directory %s",
                      directory_.c_str());
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_size,
                        value)) {
      Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_info("NiFi FlowFile Max Partition Bytes %d",
                      max_partition_bytes_);
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_time,
                        value)) {
      TimeUnit unit;
      if (Property::StringToTime(value, max_partition_millis_, unit)
          && Property::ConvertTimeUnitToMS(max_partition_millis_, unit,
                                           max_partition_millis_)) {
      }
    }
    logger_->log_info("NiFi FlowFile Max Storage Time: [%d] ms",
                      max_partition_millis_);
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, directory_.c_str(),
                                               &db_);
    if (status.ok()) {
      logger_->log_info("NiFi FlowFile Repository database open %s success",
                        directory_.c_str());
    } else {
      logger_->log_error("NiFi FlowFile Repository database open %s fail",
                         directory_.c_str());
      return false;
    }
    return true;
  }

  virtual void run();

  virtual bool Put(std::string key, uint8_t *buf, int bufLen) {

    // persistent to the DB
    leveldb::Slice value((const char *) buf, bufLen);
    leveldb::Status status;
    status = db_->Put(leveldb::WriteOptions(), key, value);
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
    leveldb::Status status;
    status = db_->Delete(leveldb::WriteOptions(), key);
    if (status.ok())
      return true;
    else
      return false;
  }
  /**
   * Sets the value from the provided key
   * @return status of the get operation.
   */
  virtual bool Get(std::string key, std::string &value) {
    leveldb::Status status;
    status = db_->Get(leveldb::ReadOptions(), key, &value);
    if (status.ok())
      return true;
    else
      return false;
  }

  void setConnectionMap(
      std::map<std::string, std::shared_ptr<minifi::Connection>> &connectionMap) {
    this->connectionMap = connectionMap;
  }
  void loadComponent();

  void start() {
    if (this->purge_period_ <= 0)
      return;
    if (running_)
      return;
    thread_ = std::thread(&FlowFileRepository::run, shared_from_this());
    thread_.detach();
    running_ = true;
    logger_->log_info("%s Repository Monitor Thread Start", name_.c_str());
  }

 private:
  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;
  leveldb::DB* db_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_FLOWFILEREPOSITORY_H_ */
