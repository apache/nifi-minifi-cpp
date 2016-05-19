/**
 * @file Connection.h
 * Connection class declaration
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
#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>

#include "FlowFileRecord.h"
#include "Relationship.h"
#include "Logger.h"

//! Forwarder declaration
class Processor;

//! Connection Class
class Connection
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	Connection(std::string name, uuid_t uuid = NULL, uuid_t srcUUID = NULL, uuid_t destUUID = NULL);
	//! Destructor
	virtual ~Connection() {}
	//! Set Connection Name
	void setName(std::string name) {
		_name = name;
	}
	//! Get Process Name
	std::string getName(void) {
		return (_name);
	}
	//! Set UUID
	void setUUID(uuid_t uuid) {
		uuid_copy(_uuid, uuid);
	}
	//! Set Source Processor UUID
	void setSourceProcessorUUID(uuid_t uuid) {
		uuid_copy(_srcUUID, uuid);
	}
	//! Set Destination Processor UUID
	void setDestinationProcessorUUID(uuid_t uuid) {
		uuid_copy(_destUUID, uuid);
	}
	//! Get Source Processor UUID
	void getSourceProcessorUUID(uuid_t uuid) {
		uuid_copy(uuid, _srcUUID);
	}
	//! Get Destination Processor UUID
	void getDestinationProcessorUUID(uuid_t uuid) {
		uuid_copy(uuid, _destUUID);
	}
	//! Get UUID
	bool getUUID(uuid_t uuid) {
		if (uuid)
		{
			uuid_copy(uuid, _uuid);
			return true;
		}
		else
			return false;
	}
	//! Set Connection Source Processor
	void setSourceProcessor(Processor *source) {
		_srcProcessor = source;
	}
	// ! Get Connection Source Processor
	Processor *getSourceProcessor() {
		return _srcProcessor;
	}
	//! Set Connection Destination Processor
	void setDestinationProcessor(Processor *dest) {
		_destProcessor = dest;
	}
	// ! Get Connection Destination Processor
	Processor *getDestinationProcessor() {
		return _destProcessor;
	}
	//! Set Connection relationship
	void setRelationship(Relationship relationship) {
		_logger->log_debug("Set connection %s relationship %s",
				_name.c_str(), relationship.getName().c_str());
		_relationship = relationship;
	}
	// ! Get Connection relationship
	Relationship getRelationship() {
		return _relationship;
	}
	//! Set Max Queue Size
	void setMaxQueueSize(uint64_t size)
	{
		_maxQueueSize = size;
	}
	//! Get Max Queue Size
	uint64_t getMaxQueueSize()
	{
		return _maxQueueSize;
	}
	//! Set Max Queue Data Size
	void setMaxQueueDataSize(uint64_t size)
	{
		_maxQueueDataSize = size;
	}
	//! Get Max Queue Data Size
	uint64_t getMaxQueueDataSize()
	{
		return _maxQueueDataSize;
	}
	//! Set Flow expiration duration in millisecond
	void setFlowExpirationDuration(uint64_t duration)
	{
		_expiredDuration = duration;
	}
	//! Get Flow expiration duration in millisecond
	uint64_t getFlowExpirationDuration()
	{
		return _expiredDuration;
	}
	//! Check whether the queue is empty
	bool isEmpty();
	//! Check whether the queue is full to apply back pressure
	bool isFull();
	//! Get queue size
	uint64_t getQueueSize() {
		std::lock_guard<std::mutex> lock(_mtx);
		return _queue.size();
	}
	//! Get queue data size
	uint64_t getQueueDataSize()
	{
		return _maxQueueDataSize;
	}
	//! Put the flow file into queue
	void put(FlowFileRecord *flow);
	//! Poll the flow file from queue, the expired flow file record also being returned
	FlowFileRecord *poll(std::set<FlowFileRecord *> &expiredFlowRecords);

protected:
	//! A global unique identifier
	uuid_t _uuid;
	//! Source Processor UUID
	uuid_t _srcUUID;
	//! Destination Processor UUID
	uuid_t _destUUID;
	//! Connection Name
	std::string _name;
	//! Relationship for this connection
	Relationship _relationship;
	//! Source Processor (ProcessNode/Port)
	Processor *_srcProcessor;
	//! Destination Processor (ProcessNode/Port)
	Processor *_destProcessor;
	//! Max queue size to apply back pressure
	std::atomic<uint64_t> _maxQueueSize;
	//! Max queue data size to apply back pressure
	std::atomic<uint64_t> _maxQueueDataSize;
	//! Flow File Expiration Duration in= MilliSeconds
	std::atomic<uint64_t> _expiredDuration;


private:
	//! Mutex for protection
	std::mutex _mtx;
	//! Queued data size
	std::atomic<uint64_t> _queuedDataSize;
	//! Queue for the Flow File
	std::queue<FlowFileRecord *> _queue;
	//! Logger
	Logger *_logger;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	Connection(const Connection &parent);
	Connection &operator=(const Connection &parent);

};

#endif
