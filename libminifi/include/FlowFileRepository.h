/**
 * @file FlowFileRepository 
 * Flow file repository class declaration
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
#ifndef __FLOWFILE_REPOSITORY_H__
#define __FLOWFILE_REPOSITORY_H__

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

#include "Configure.h"
#include "Connection.h"
#include "FlowFileRecord.h"
#include "Logger.h"
#include "Property.h"
#include "ResourceClaim.h"
#include "io/Serializable.h"
#include "utils/TimeUtil.h"
#include "Repository.h"

class FlowFileRepository;

//! FlowFile Event Record
class FlowFileEventRecord : protected Serializable
{
public:
	friend class ProcessSession;
public:
	//! Constructor
	/*!
	 * Create a new provenance event record
	 */
	FlowFileEventRecord()
	: _entryDate(0), _lineageStartDate(0), _size(0), _offset(0)  
	{
		_eventTime = getTimeMillis();
		logger_ = Logger::getLogger();
	}

	//! Destructor
	virtual ~FlowFileEventRecord() {
	}
	//! Get Attributes
	std::map<std::string, std::string> getAttributes() {
		return _attributes;
	}
	//! Get Size
	uint64_t getFileSize() {
		return _size;
	}
	// ! Get Offset
	uint64_t getFileOffset() {
		return _offset;
	}
	// ! Get Entry Date
	uint64_t getFlowFileEntryDate() {
		return _entryDate;
	}
	// ! Get Lineage Start Date
	uint64_t getlineageStartDate() {
		return _lineageStartDate;
	}
	// ! Get Event Time
	uint64_t getEventTime() {
		return _eventTime;
	}
	//! Get FlowFileUuid
	std::string getFlowFileUuid()
	{
		return _uuid;
	}
	//! Get ConnectionUuid
	std::string getConnectionUuid()
	{
		return _uuidConnection;
	}
	//! Get content full path
	std::string getContentFullPath()
	{
		return _contentFullPath;
	}
	//! Get LineageIdentifiers
	std::set<std::string> getLineageIdentifiers()
	{
		return _lineageIdentifiers;
	}
	//! fromFlowFile
	void fromFlowFile(FlowFileRecord *flow, std::string uuidConnection)
	{
		_entryDate = flow->getEntryDate();
		_lineageStartDate = flow->getlineageStartDate();
		_lineageIdentifiers = flow->getlineageIdentifiers();
		_uuid = flow->getUUIDStr();
		_attributes = flow->getAttributes();
		_size = flow->getSize();
		_offset = flow->getOffset();
		_uuidConnection = uuidConnection;
		if (flow->getResourceClaim())
		{
			_contentFullPath = flow->getResourceClaim()->getContentFullPath();
		}
	}
	//! Serialize and Persistent to the repository
	bool Serialize(FlowFileRepository *repo);
	//! DeSerialize
	bool DeSerialize(const uint8_t *buffer, const int bufferSize);
	//! DeSerialize
	bool DeSerialize(DataStream &stream)
	{
		return DeSerialize(stream.getBuffer(),stream.getSize());
	}
	//! DeSerialize
	bool DeSerialize(FlowFileRepository *repo, std::string key);

protected:

	//! Date at which the event was created
	uint64_t _eventTime;
	//! Date at which the flow file entered the flow
	uint64_t _entryDate;
	//! Date at which the origin of this flow file entered the flow
	uint64_t _lineageStartDate;
	//! Size in bytes of the data corresponding to this flow file
	uint64_t _size;
	//! flow uuid
	std::string _uuid;
	//! connection uuid
	std::string _uuidConnection;
	//! Offset to the content
	uint64_t _offset;
	//! Full path to the content
	std::string _contentFullPath;
	//! Attributes key/values pairs for the flow record
	std::map<std::string, std::string> _attributes;
	//! UUID string for all parents
	std::set<std::string> _lineageIdentifiers;

private:

	//! Logger
	std::shared_ptr<Logger> logger_;
	
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	FlowFileEventRecord(const FlowFileEventRecord &parent);
	FlowFileEventRecord &operator=(const FlowFileEventRecord &parent);

};

#define FLOWFILE_REPOSITORY_DIRECTORY "./flowfile_repository"
#define MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME (600000) // 10 minute
#define FLOWFILE_REPOSITORY_PURGE_PERIOD (2500) // 2500 msec

//! FlowFile Repository
class FlowFileRepository : public Repository
{
public:
	//! Constructor
	/*!
	 * Create a new provenance repository
	 */
	FlowFileRepository()
	 : Repository(Repository::FLOWFILE, FLOWFILE_REPOSITORY_DIRECTORY,
			MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME, MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, FLOWFILE_REPOSITORY_PURGE_PERIOD)
	{
	}
	//! Destructor
	virtual ~FlowFileRepository() {
	}
	//! Load Repo to Connections
	void loadFlowFileToConnections(std::map<std::string, Connection *> *connectionMap);

protected:

private:

	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	FlowFileRepository(const FlowFileRepository &parent);
	FlowFileRepository &operator=(const FlowFileRepository &parent);
};

#endif
