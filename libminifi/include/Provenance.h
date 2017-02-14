/**
 * @file Provenance.h
 * Flow file record class declaration
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
#ifndef __PROVENANCE_H__
#define __PROVENANCE_H__

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
#include "TimeUtil.h"
#include "Serializable.h"

// Provenance Event Record Serialization Seg Size
#define PROVENANCE_EVENT_RECORD_SEG_SIZE 2048

class ProvenanceRepository;

//! Provenance Event Record
class ProvenanceEventRecord : protected Serializable
{
public:
	enum ProvenanceEventType {

	    /**
	     * A CREATE event is used when a FlowFile is generated from data that was
	     * not received from a remote system or external process
	     */
	    CREATE,

	    /**
	     * Indicates a provenance event for receiving data from an external process. This Event Type
	     * is expected to be the first event for a FlowFile. As such, a Processor that receives data
	     * from an external source and uses that data to replace the content of an existing FlowFile
	     * should use the {@link #FETCH} event type, rather than the RECEIVE event type.
	     */
	    RECEIVE,

	    /**
	     * Indicates that the contents of a FlowFile were overwritten using the contents of some
	     * external resource. This is similar to the {@link #RECEIVE} event but varies in that
	     * RECEIVE events are intended to be used as the event that introduces the FlowFile into
	     * the system, whereas FETCH is used to indicate that the contents of an existing FlowFile
	     * were overwritten.
	     */
	    FETCH,

	    /**
	     * Indicates a provenance event for sending data to an external process
	     */
	    SEND,

	    /**
	     * Indicates that the contents of a FlowFile were downloaded by a user or external entity.
	     */
	    DOWNLOAD,

	    /**
	     * Indicates a provenance event for the conclusion of an object's life for
	     * some reason other than object expiration
	     */
	    DROP,

	    /**
	     * Indicates a provenance event for the conclusion of an object's life due
	     * to the fact that the object could not be processed in a timely manner
	     */
	    EXPIRE,

	    /**
	     * FORK is used to indicate that one or more FlowFile was derived from a
	     * parent FlowFile.
	     */
	    FORK,

	    /**
	     * JOIN is used to indicate that a single FlowFile is derived from joining
	     * together multiple parent FlowFiles.
	     */
	    JOIN,

	    /**
	     * CLONE is used to indicate that a FlowFile is an exact duplicate of its
	     * parent FlowFile.
	     */
	    CLONE,

	    /**
	     * CONTENT_MODIFIED is used to indicate that a FlowFile's content was
	     * modified in some way. When using this Event Type, it is advisable to
	     * provide details about how the content is modified.
	     */
	    CONTENT_MODIFIED,

	    /**
	     * ATTRIBUTES_MODIFIED is used to indicate that a FlowFile's attributes were
	     * modified in some way. This event is not needed when another event is
	     * reported at the same time, as the other event will already contain all
	     * FlowFile attributes.
	     */
	    ATTRIBUTES_MODIFIED,

	    /**
	     * ROUTE is used to show that a FlowFile was routed to a specified
	     * {@link org.apache.nifi.processor.Relationship Relationship} and should provide
	     * information about why the FlowFile was routed to this relationship.
	     */
	    ROUTE,

	    /**
	     * Indicates a provenance event for adding additional information such as a
	     * new linkage to a new URI or UUID
	     */
	    ADDINFO,

	    /**
	     * Indicates a provenance event for replaying a FlowFile. The UUID of the
	     * event will indicate the UUID of the original FlowFile that is being
	     * replayed. The event will contain exactly one Parent UUID that is also the
	     * UUID of the FlowFile that is being replayed and exactly one Child UUID
	     * that is the UUID of the a newly created FlowFile that will be re-queued
	     * for processing.
	     */
	    REPLAY
	};
	friend class ProcessSession;
public:
	//! Constructor
	/*!
	 * Create a new provenance event record
	 */
	ProvenanceEventRecord(ProvenanceEventType event, std::string componentId, std::string componentType) {
		_eventType = event;
		_componentId = componentId;
		_componentType = componentType;
		_eventTime = getTimeMillis();
		char eventIdStr[37];
		// Generate the global UUID for th event
		uuid_generate(_eventId);
		uuid_unparse(_eventId, eventIdStr);
		_eventIdStr = eventIdStr;
		_logger = Logger::getLogger();
	}

	ProvenanceEventRecord() {
			_eventTime = getTimeMillis();
			_logger = Logger::getLogger();
	}

	//! Destructor
	virtual ~ProvenanceEventRecord() {
	}
	//! Get the Event ID
	std::string getEventId() {
		return _eventIdStr;
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
	// ! Get Event Duration
	uint64_t getEventDuration() {
		return _eventDuration;
	}
	//! Set Event Duration
	void setEventDuration(uint64_t duration)
	{
		_eventDuration = duration;
	}
	// ! Get Event Type
	ProvenanceEventType getEventType() {
		return _eventType;
	}
	//! Get Component ID
	std::string getComponentId()
	{
		return _componentId;
	}
	//! Get Component Type
	std::string getComponentType()
	{
		return _componentType;
	}
	//! Get FlowFileUuid
	std::string getFlowFileUuid()
	{
		return _uuid;
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
	//! Get Details
	std::string getDetails()
	{
		return _details;
	}
	//! Set Details
	void setDetails(std::string details)
	{
		_details = details;
	}
	//! Get TransitUri
	std::string getTransitUri()
	{
		return _transitUri;
	}
	//! Set TransitUri
	void setTransitUri(std::string uri)
	{
		_transitUri = uri;
	}
	//! Get SourceSystemFlowFileIdentifier
	std::string getSourceSystemFlowFileIdentifier()
	{
		return _sourceSystemFlowFileIdentifier;
	}
	//! Set SourceSystemFlowFileIdentifier
	void setSourceSystemFlowFileIdentifier(std::string identifier)
	{
		_sourceSystemFlowFileIdentifier = identifier;
	}
	//! Get Parent UUIDs
	std::vector<std::string> getParentUuids()
	{
		return _parentUuids;
	}
	//! Add Parent UUID
	void addParentUuid(std::string uuid)
	{
		if (std::find(_parentUuids.begin(), _parentUuids.end(), uuid) != _parentUuids.end())
			return;
		else
			_parentUuids.push_back(uuid);
	}
	//! Add Parent Flow File
	void addParentFlowFile(FlowFileRecord *flow)
	{
		addParentUuid(flow->getUUIDStr());
		return;
	}
	//! Remove Parent UUID
	void removeParentUuid(std::string uuid)
	{
		_parentUuids.erase(std::remove(_parentUuids.begin(), _parentUuids.end(), uuid), _parentUuids.end());
	}
	//! Remove Parent Flow File
	void removeParentFlowFile(FlowFileRecord *flow)
	{
		removeParentUuid(flow->getUUIDStr());
		return;
	}
	//! Get Children UUIDs
	std::vector<std::string> getChildrenUuids()
	{
		return _childrenUuids;
	}
	//! Add Child UUID
	void addChildUuid(std::string uuid)
	{
		if (std::find(_childrenUuids.begin(), _childrenUuids.end(), uuid) != _childrenUuids.end())
			return;
		else
			_childrenUuids.push_back(uuid);
	}
	//! Add Child Flow File
	void addChildFlowFile(FlowFileRecord *flow)
	{
		addChildUuid(flow->getUUIDStr());
		return;
	}
	//! Remove Child UUID
	void removeChildUuid(std::string uuid)
	{
		_childrenUuids.erase(std::remove(_childrenUuids.begin(), _childrenUuids.end(), uuid), _childrenUuids.end());
	}
	//! Remove Child Flow File
	void removeChildFlowFile(FlowFileRecord *flow)
	{
		removeChildUuid(flow->getUUIDStr());
		return;
	}
	//! Get AlternateIdentifierUri
	std::string getAlternateIdentifierUri()
	{
		return _alternateIdentifierUri;
	}
	//! Set AlternateIdentifierUri
	void setAlternateIdentifierUri(std::string uri)
	{
		_alternateIdentifierUri = uri;
	}
	//! Get Relationship
	std::string getRelationship()
	{
		return _relationship;
	}
	//! Set Relationship
	void setRelationship(std::string relation)
	{
		_relationship = relation;
	}
	//! Get sourceQueueIdentifier
	std::string getSourceQueueIdentifier()
	{
		return _sourceQueueIdentifier;
	}
	//! Set sourceQueueIdentifier
	void setSourceQueueIdentifier(std::string identifier)
	{
		_sourceQueueIdentifier = identifier;
	}
	//! fromFlowFile
	void fromFlowFile(FlowFileRecord *flow)
	{
		_entryDate = flow->getEntryDate();
		_lineageStartDate = flow->getlineageStartDate();
		_lineageIdentifiers = flow->getlineageIdentifiers();
		_uuid = flow->getUUIDStr();
		_attributes = flow->getAttributes();
		_size = flow->getSize();
		_offset = flow->getOffset();
		if (flow->getOriginalConnection())
			_sourceQueueIdentifier = flow->getOriginalConnection()->getName();
		if (flow->getResourceClaim())
		{
			_contentFullPath = flow->getResourceClaim()->getContentFullPath();
		}
	}
	//! Serialize and Persistent to the repository
	bool Serialize(ProvenanceRepository *repo);
	//! DeSerialize
	bool DeSerialize(const uint8_t *buffer, const int bufferSize);
	//! DeSerialize
	bool DeSerialize(DataStream &stream)
	{
		return DeSerialize(stream.getBuffer(),stream.getSize());
	}
	//! DeSerialize
	bool DeSerialize(ProvenanceRepository *repo, std::string key);

protected:

	//! Event type
	ProvenanceEventType _eventType;
	//! Date at which the event was created
	uint64_t _eventTime;
	//! Date at which the flow file entered the flow
	uint64_t _entryDate;
	//! Date at which the origin of this flow file entered the flow
	uint64_t _lineageStartDate;
	//! Event Duration
	uint64_t _eventDuration;
	//! Component ID
	std::string _componentId;
	//! Component Type
	std::string _componentType;
	//! Size in bytes of the data corresponding to this flow file
	uint64_t _size;
	//! flow uuid
	std::string _uuid;
	//! Offset to the content
	uint64_t _offset;
	//! Full path to the content
	std::string _contentFullPath;
	//! Attributes key/values pairs for the flow record
	std::map<std::string, std::string> _attributes;
	//! provenance ID
	uuid_t _eventId;
	//! UUID string for all parents
	std::set<std::string> _lineageIdentifiers;
	//! transitUri
	std::string _transitUri;
	//! sourceSystemFlowFileIdentifier
	std::string _sourceSystemFlowFileIdentifier;
	//! parent UUID
	std::vector<std::string> _parentUuids;
	//! child UUID
	std::vector<std::string> _childrenUuids;
	//! detail
	std::string _details;
	//! sourceQueueIdentifier
	std::string _sourceQueueIdentifier;
	//! event ID Str
	std::string _eventIdStr;
	//! relationship
	std::string _relationship;
	//! alternateIdentifierUri;
	std::string _alternateIdentifierUri;

private:

	//! Logger
	Logger *_logger;
	
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProvenanceEventRecord(const ProvenanceEventRecord &parent);
	ProvenanceEventRecord &operator=(const ProvenanceEventRecord &parent);

};

//! Provenance Reporter
class ProvenanceReporter
{
	friend class ProcessSession;
public:
	//! Constructor
	/*!
	 * Create a new provenance reporter associated with the process session
	 */
	ProvenanceReporter(std::string componentId, std::string componentType) {
		_logger = Logger::getLogger();
		_componentId = componentId;
		_componentType = componentType;
	}

	//! Destructor
	virtual ~ProvenanceReporter() {
		clear();
	}
	//! Get events
	std::set<ProvenanceEventRecord *> getEvents()
	{
		return _events;
	}
	//! Add event
	void add(ProvenanceEventRecord *event)
	{
		_events.insert(event);
	}
	//! Remove event
	void remove(ProvenanceEventRecord *event)
	{
		if (_events.find(event) != _events.end())
		{
			_events.erase(event);
		}
	}
	//!
	//! clear
	void clear()
	{
		for (auto it : _events)
		{
			delete it;
		}
		_events.clear();
	}
	//! allocate
	ProvenanceEventRecord *allocate(ProvenanceEventRecord::ProvenanceEventType eventType, FlowFileRecord *flow)
	{
		ProvenanceEventRecord *event = new ProvenanceEventRecord(eventType, _componentId, _componentType);
		if (event)
			event->fromFlowFile(flow);

		return event;
	}
	//! commit
	void commit();
	//! create
	void create(FlowFileRecord *flow, std::string detail);
	//! route
	void route(FlowFileRecord *flow, Relationship relation, std::string detail, uint64_t processingDuration);
	//! modifyAttributes
	void modifyAttributes(FlowFileRecord *flow, std::string detail);
	//! modifyContent
	void modifyContent(FlowFileRecord *flow, std::string detail, uint64_t processingDuration);
	//! clone
	void clone(FlowFileRecord *parent, FlowFileRecord *child);
	//! join
	void join(std::vector<FlowFileRecord *> parents, FlowFileRecord *child, std::string detail, uint64_t processingDuration);
	//! fork
	void fork(std::vector<FlowFileRecord *> child, FlowFileRecord *parent, std::string detail, uint64_t processingDuration);
	//! expire
	void expire(FlowFileRecord *flow, std::string detail);
	//! drop
	void drop(FlowFileRecord *flow, std::string reason);
	//! send
	void send(FlowFileRecord *flow, std::string transitUri, std::string detail, uint64_t processingDuration, bool force);
	//! fetch
	void fetch(FlowFileRecord *flow, std::string transitUri, std::string detail, uint64_t processingDuration);
	//! receive
	void receive(FlowFileRecord *flow, std::string transitUri, std::string sourceSystemFlowFileIdentifier, std::string detail, uint64_t processingDuration);

protected:

	//! Component ID
	std::string _componentId;
	//! Component Type
	std::string _componentType;

private:

	//! Incoming connection Iterator
	std::set<ProvenanceEventRecord *> _events;
	//! Logger
	Logger *_logger;

	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProvenanceReporter(const ProvenanceReporter &parent);
	ProvenanceReporter &operator=(const ProvenanceReporter &parent);
};

#define PROVENANCE_DIRECTORY "./provenance_repository"
#define MAX_PROVENANCE_STORAGE_SIZE (10*1024*1024) // 10M
#define MAX_PROVENANCE_ENTRY_LIFE_TIME (60000) // 1 minute
#define PROVENANCE_PURGE_PERIOD (2500) // 2500 msec

//! Provenance Repository
class ProvenanceRepository
{
public:
	//! Constructor
	/*!
	 * Create a new provenance repository
	 */
	ProvenanceRepository() {
		_logger = Logger::getLogger();
		_configure = Configure::getConfigure();
		_directory = PROVENANCE_DIRECTORY;
		_maxPartitionMillis = MAX_PROVENANCE_ENTRY_LIFE_TIME;
		_purgePeriod = PROVENANCE_PURGE_PERIOD;
		_maxPartitionBytes = MAX_PROVENANCE_STORAGE_SIZE;
		_db = NULL;
		_thread = NULL;
		_running = false;
		_repoFull = false;
	}

	//! Destructor
	virtual ~ProvenanceRepository() {
		stop();
		if (this->_thread)
			delete this->_thread;
		destroy();
	}

	//! initialize
	virtual bool initialize()
	{
		std::string value;
		if (_configure->get(Configure::nifi_provenance_repository_directory_default, value))
		{
			_directory = value;
		}
		_logger->log_info("NiFi Provenance Repository Directory %s", _directory.c_str());
		if (_configure->get(Configure::nifi_provenance_repository_max_storage_size, value))
		{
			Property::StringToInt(value, _maxPartitionBytes);
		}
		_logger->log_info("NiFi Provenance Max Partition Bytes %d", _maxPartitionBytes);
		if (_configure->get(Configure::nifi_provenance_repository_max_storage_time, value))
		{
			TimeUnit unit;
			if (Property::StringToTime(value, _maxPartitionMillis, unit) &&
						Property::ConvertTimeUnitToMS(_maxPartitionMillis, unit, _maxPartitionMillis))
			{
			}
		}
		_logger->log_info("NiFi Provenance Max Storage Time: [%d] ms", _maxPartitionMillis);
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status = leveldb::DB::Open(options, _directory.c_str(), &_db);
		if (status.ok())
		{
			_logger->log_info("NiFi Provenance Repository database open %s success", _directory.c_str());
		}
		else
		{
			_logger->log_error("NiFi Provenance Repository database open %s fail", _directory.c_str());
			return false;
		}

		// start the monitor thread
		start();
		return true;
	}
	//! Put
	virtual bool Put(std::string key, uint8_t *buf, int bufLen)
	{
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
		leveldb::Status status;
		status = _db->Get(leveldb::ReadOptions(), key, &value);
		if (status.ok())
			return true;
		else
			return false;
	}
	//! Persistent event
	void registerEvent(ProvenanceEventRecord *event)
	{
		event->Serialize(this);
	}
	//! Remove event
	void removeEvent(ProvenanceEventRecord *event)
	{
		Delete(event->getEventId());
	}
	//! destroy
	void destroy()
	{
		if (_db)
		{
			delete _db;
			_db = NULL;
		}
	}
	//! Run function for the thread
	static void run(ProvenanceRepository *repo);
	//! Start the repository monitor thread
	virtual void start();
	//! Stop the repository monitor thread
	virtual void stop();
	//! whether the repo is full
	virtual bool isFull()
	{
		return _repoFull;
	}

protected:

private:

	//! Mutex for protection
	std::mutex _mtx;
	//! repository directory
	std::string _directory;
	//! Logger
	Logger *_logger;
	//! Configure
	//! max db entry life time
	Configure *_configure;
	int64_t _maxPartitionMillis;
	//! max db size
	int64_t _maxPartitionBytes;
	//! purge period
	uint64_t _purgePeriod;
	//! level DB database
	leveldb::DB* _db;
	//! thread
	std::thread *_thread;
	//! whether it is running
	bool _running;
	//! whether stop accepting provenace event
	std::atomic<bool> _repoFull;
	//! size of the directory
	static uint64_t _repoSize;
	//! call back for directory size
	static int repoSum(const char *fpath, const struct stat *sb, int typeflag)
	{
	    _repoSize += sb->st_size;
	    return 0;
	}
	//! repoSize
	uint64_t repoSize()
	{
		_repoSize = 0;
		if (ftw(_directory.c_str(), repoSum, 1) != 0)
			_repoSize = 0;

		return _repoSize;
	}
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProvenanceRepository(const ProvenanceRepository &parent);
	ProvenanceRepository &operator=(const ProvenanceRepository &parent);
};




#endif
