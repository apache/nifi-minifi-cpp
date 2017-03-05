/**
 * @file Repository 
 * Repository class declaration
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
#ifndef __REPOSITORY_H__
#define __REPOSITORY_H__

#include <ftw.h>
#include <uuid/uuid.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "Configure.h"
#include "Connection.h"
#include "FlowFileRecord.h"
#include "Logger.h"
#include "Property.h"
#include "ResourceClaim.h"
#include "io/Serializable.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"

//! Repository
class Repository
{
public:
	enum RepositoryType {
		//! Provenance Repo Type
		PROVENANCE,
		//! FlowFile Repo Type
		FLOWFILE,
		MAX_REPO_TYPE
	};
	static const char *RepositoryTypeStr[MAX_REPO_TYPE];
	//! Constructor
	/*!
	 * Create a new provenance repository
	 */
	Repository(RepositoryType type, std::string directory, 
		int64_t maxPartitionMillis, int64_t maxPartitionBytes, uint64_t purgePeriod) {
		_type = type;
		_directory = directory;
		_maxPartitionMillis = maxPartitionMillis;
		_maxPartitionBytes = maxPartitionBytes;
		_purgePeriod = purgePeriod;
		logger_ = Logger::getLogger();
		configure_ = Configure::getConfigure();
		_db = NULL;
		_thread = NULL;
		_running = false;
		_repoFull = false;
		_enable = true;
	}

	//! Destructor
	virtual ~Repository() {
		stop();
		if (this->_thread)
			delete this->_thread;
		destroy();
	}

	//! initialize
	virtual bool initialize()
	{
		std::string value;

		if (_type == PROVENANCE)
		{
			if (!(configure_->get(Configure::nifi_provenance_repository_enable, value)
					&& StringUtils::StringToBool(value, _enable))) {
				_enable = true;
			}
			if (!_enable)
				return false;
			if (configure_->get(Configure::nifi_provenance_repository_directory_default, value))
			{
				_directory = value;
			}
			logger_->log_info("NiFi Provenance Repository Directory %s", _directory.c_str());
			if (configure_->get(Configure::nifi_provenance_repository_max_storage_size, value))
			{
				Property::StringToInt(value, _maxPartitionBytes);
			}
			logger_->log_info("NiFi Provenance Max Partition Bytes %d", _maxPartitionBytes);
			if (configure_->get(Configure::nifi_provenance_repository_max_storage_time, value))
			{
				TimeUnit unit;
				if (Property::StringToTime(value, _maxPartitionMillis, unit) &&
							Property::ConvertTimeUnitToMS(_maxPartitionMillis, unit, _maxPartitionMillis))
				{
				}
			}
			logger_->log_info("NiFi Provenance Max Storage Time: [%d] ms", _maxPartitionMillis);
			leveldb::Options options;
			options.create_if_missing = true;
			leveldb::Status status = leveldb::DB::Open(options, _directory.c_str(), &_db);
			if (status.ok())
			{
				logger_->log_info("NiFi Provenance Repository database open %s success", _directory.c_str());
			}
			else
			{
				logger_->log_error("NiFi Provenance Repository database open %s fail", _directory.c_str());
				return false;
			}
		}

		if (_type == FLOWFILE)
		{
			if (!(configure_->get(Configure::nifi_flowfile_repository_enable, value)
					&& StringUtils::StringToBool(value, _enable))) {
				_enable = true;
			}
			if (!_enable)
				return false;
			if (configure_->get(Configure::nifi_flowfile_repository_directory_default, value))
			{
				_directory = value;
			}
			logger_->log_info("NiFi FlowFile Repository Directory %s", _directory.c_str());
			if (configure_->get(Configure::nifi_flowfile_repository_max_storage_size, value))
			{
				Property::StringToInt(value, _maxPartitionBytes);
			}
			logger_->log_info("NiFi FlowFile Max Partition Bytes %d", _maxPartitionBytes);
			if (configure_->get(Configure::nifi_flowfile_repository_max_storage_time, value))
			{
				TimeUnit unit;
				if (Property::StringToTime(value, _maxPartitionMillis, unit) &&
							Property::ConvertTimeUnitToMS(_maxPartitionMillis, unit, _maxPartitionMillis))
				{
				}
			}
			logger_->log_info("NiFi FlowFile Max Storage Time: [%d] ms", _maxPartitionMillis);
			leveldb::Options options;
			options.create_if_missing = true;
			leveldb::Status status = leveldb::DB::Open(options, _directory.c_str(), &_db);
			if (status.ok())
			{
				logger_->log_info("NiFi FlowFile Repository database open %s success", _directory.c_str());
			}
			else
			{
				logger_->log_error("NiFi FlowFile Repository database open %s fail", _directory.c_str());
				return false;
			}
		}

		return true;
	}
	//! Put
	virtual bool Put(std::string key, uint8_t *buf, int bufLen)
	{
		if (!_enable)
			return false;
			
		// persistent to the DB
		leveldb::Slice value((const char *) buf, bufLen);
		leveldb::Status status;
		status = _db->Put(leveldb::WriteOptions(), key, value);
		if (status.ok())
			return true;
		else
			return false;
	}
	//! Delete
	virtual bool Delete(std::string key)
	{
		if (!_enable)
			return false;
		leveldb::Status status;
		status = _db->Delete(leveldb::WriteOptions(), key);
		if (status.ok())
			return true;
		else
			return false;
	}
	//! Get
	virtual bool Get(std::string key, std::string &value)
	{
		if (!_enable)
			return false;
		leveldb::Status status;
		status = _db->Get(leveldb::ReadOptions(), key, &value);
		if (status.ok())
			return true;
		else
			return false;
	}
	//! Run function for the thread
	static void run(Repository *repo);
	//! Start the repository monitor thread
	virtual void start();
	//! Stop the repository monitor thread
	virtual void stop();
	//! whether the repo is full
	virtual bool isFull()
	{
		return _repoFull;
	}
	//! whether the repo is enable 
	virtual bool isEnable()
	{
		return _enable;
	}

protected:
	//! Repo Type
	RepositoryType _type;
	//! Mutex for protection
	std::mutex _mtx;
	//! repository directory
	std::string _directory;
	//! Logger
	std::shared_ptr<Logger> logger_;
	//! Configure
	//! max db entry life time
	Configure *configure_;
	int64_t _maxPartitionMillis;
	//! max db size
	int64_t _maxPartitionBytes;
	//! purge period
	uint64_t _purgePeriod;
	//! level DB database
	leveldb::DB* _db;
	//! thread
	std::thread *_thread;
	//! whether the monitoring thread is running for the repo while it was enabled 
	bool _running;
	//! whether it is enabled by minfi property for the repo 
	bool _enable;
	//! whether stop accepting provenace event
	std::atomic<bool> _repoFull;
	//! repoSize
	uint64_t repoSize();
	//! size of the directory
	static uint64_t _repoSize[MAX_REPO_TYPE];
	//! call back for directory size
	static int repoSumProvenance(const char *fpath, const struct stat *sb, int typeflag)
	{
		_repoSize[PROVENANCE] += sb->st_size;
		return 0;
	}
	//! call back for directory size
	static int repoSumFlowFile(const char *fpath, const struct stat *sb, int typeflag)
	{
		_repoSize[FLOWFILE] += sb->st_size;
		return 0;
	}

private:
	//! destroy
	void destroy()
	{
		if (_db)
		{
			delete _db;
			_db = NULL;
		}
	}
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	Repository(const Repository &parent);
	Repository &operator=(const Repository &parent);
};

#endif
