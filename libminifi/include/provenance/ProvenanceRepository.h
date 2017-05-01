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

#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "provenance/Provenance.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

#define PROVENANCE_DIRECTORY "./provenance_repository"
#define MAX_PROVENANCE_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_PROVENANCE_ENTRY_LIFE_TIME (60000) // 1 minute
#define PROVENANCE_PURGE_PERIOD (2500) // 2500 msec

class ProvenanceRepository : public core::Repository,
    public std::enable_shared_from_this<ProvenanceRepository> {
 public:
  // Constructor
  /*!
   * Create a new provenance repository
   */
  ProvenanceRepository(std::string directory = PROVENANCE_DIRECTORY,
                       int64_t maxPartitionMillis =
                       MAX_PROVENANCE_ENTRY_LIFE_TIME,
                       int64_t maxPartitionBytes = MAX_PROVENANCE_STORAGE_SIZE,
                       uint64_t purgePeriod = PROVENANCE_PURGE_PERIOD)
      : Repository(core::getClassName<ProvenanceRepository>(), directory,
                   maxPartitionMillis, maxPartitionBytes, purgePeriod) {

    db_ = NULL;
  }

  // Destructor
  virtual ~ProvenanceRepository() {
    if (db_)
      delete db_;
  }
  
  void start() {
  if (this->purge_period_ <= 0)
    return;
  if (running_)
    return;
  thread_ = std::thread(&ProvenanceRepository::run, shared_from_this());
  thread_.detach();
  running_ = true;
  logger_->log_info("%s Repository Monitor Thread Start", name_.c_str());
}

  // initialize
  virtual bool initialize(std::shared_ptr<org::apache::nifi::minifi::Configure> config) {
    std::string value;
    if (config->get(Configure::nifi_provenance_repository_directory_default,
                        value)) {
      directory_ = value;
    }
    logger_->log_info("NiFi Provenance Repository Directory %s",
                      directory_.c_str());
    if (config->get(Configure::nifi_provenance_repository_max_storage_size,
                        value)) {
      core::Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_info("NiFi Provenance Max Partition Bytes %d",
                      max_partition_bytes_);
    if (config->get(Configure::nifi_provenance_repository_max_storage_time,
                        value)) {
      core::TimeUnit unit;
      if (core::Property::StringToTime(value, max_partition_millis_, unit)
          && core::Property::ConvertTimeUnitToMS(max_partition_millis_, unit,
                                                 max_partition_millis_)) {
      }
    }
    logger_->log_info("NiFi Provenance Max Storage Time: [%d] ms",
                      max_partition_millis_);
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, directory_.c_str(),
                                               &db_);
    if (status.ok()) {
      logger_->log_info("NiFi Provenance Repository database open %s success",
                        directory_.c_str());
    } else {
      logger_->log_error("NiFi Provenance Repository database open %s fail",
                         directory_.c_str());
      return false;
    }

    return true;
  }
  // Put
  virtual bool Put(std::string key, uint8_t *buf, int bufLen) {

	if (repo_full_)
		return false;

    // persistent to the DB
    leveldb::Slice value((const char *) buf, bufLen);
    leveldb::Status status;
    status = db_->Put(leveldb::WriteOptions(), key, value);
    if (status.ok())
      return true;
    else
      return false;
  }
  // Delete
  virtual bool Delete(std::string key) {
    leveldb::Status status;
    status = db_->Delete(leveldb::WriteOptions(), key);
    if (status.ok())
      return true;
    else
      return false;
  }
  // Get
  virtual bool Get(std::string key, std::string &value) {
    leveldb::Status status;
    status = db_->Get(leveldb::ReadOptions(), key, &value);
    if (status.ok())
      return true;
    else
      return false;
  }
  // Persistent event
  void registerEvent(std::shared_ptr<ProvenanceEventRecord> &event) {
    event->Serialize(
        std::static_pointer_cast<core::Repository>(shared_from_this()));
  }
  // Remove event
  void removeEvent(ProvenanceEventRecord *event) {
    Delete(event->getEventId());
  }
  //! get record
  void getProvenanceRecord(std::vector<std::shared_ptr<ProvenanceEventRecord>> &records, int maxSize)
  {
	std::lock_guard<std::mutex> lock(mutex_);
	leveldb::Iterator* it = db_->NewIterator(
				leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
			std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
			std::string key = it->key().ToString();
			if (records.size() >= maxSize)
				break;
			if (eventRead->DeSerialize((uint8_t *) it->value().data(),
					(int) it->value().size()))
			{
				records.push_back(eventRead);
			}
	}
	delete it;
  }
  //! purge record
  void purgeProvenanceRecord(std::vector<std::shared_ptr<ProvenanceEventRecord>> &records)
  {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto record : records)
	{
		Delete(record->getEventId());
	}
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
  leveldb::DB* db_;

};

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCEREPOSITORY_H_ */

