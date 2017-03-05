/**
 * @file FlowFileRecord.h
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
#ifndef __FLOW_FILE_RECORD_H__
#define __FLOW_FILE_RECORD_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>

#include "utils/TimeUtil.h"
#include "Logger.h"
#include "ResourceClaim.h"

class ProcessSession;
class Connection;
class FlowFileEventRecord;

#define DEFAULT_FLOWFILE_PATH "."

//! FlowFile Attribute
enum FlowAttribute
{
	//! The flowfile's path indicates the relative directory to which a FlowFile belongs and does not contain the filename
	PATH = 0,
	//! The flowfile's absolute path indicates the absolute directory to which a FlowFile belongs and does not contain the filename
	ABSOLUTE_PATH,
	//! The filename of the FlowFile. The filename should not contain any directory structure.
	FILENAME,
	//! A unique UUID assigned to this FlowFile.
	UUID,
	//! A numeric value indicating the FlowFile priority
	priority,
	//! The MIME Type of this FlowFile
	MIME_TYPE,
	//! Specifies the reason that a FlowFile is being discarded
	DISCARD_REASON,
	//! Indicates an identifier other than the FlowFile's UUID that is known to refer to this FlowFile.
	ALTERNATE_IDENTIFIER,
	MAX_FLOW_ATTRIBUTES
};

//! FlowFile Attribute Key
static const char *FlowAttributeKeyArray[MAX_FLOW_ATTRIBUTES] =
{
		"path",
		"absolute.path",
		"filename",
		"uuid",
		"priority",
		"mime.type",
		"discard.reason",
		"alternate.identifier"
};

//! FlowFile Attribute Enum to Key
inline const char *FlowAttributeKey(FlowAttribute attribute)
{
	if (attribute < MAX_FLOW_ATTRIBUTES)
		return FlowAttributeKeyArray[attribute];
	else
		return NULL;
}

//! FlowFile IO Callback functions for input and output
//! throw exception for error
class InputStreamCallback
{
public:
	virtual void process(std::ifstream *stream) = 0;
};
class OutputStreamCallback
{
public:
	virtual void process(std::ofstream *stream) = 0;
};


//! FlowFile Record Class
class FlowFileRecord
{
	friend class ProcessSession;
public:
	//! Constructor
	/*!
	 * Create a new flow record
	 */
	explicit FlowFileRecord(std::map<std::string, std::string> attributes, ResourceClaim *claim = NULL);
	/*!
	 * Create a new flow record from repo flow event
	 */
	explicit FlowFileRecord(FlowFileEventRecord *event);
	//! Destructor
	virtual ~FlowFileRecord();
	//! addAttribute key is enum
	bool addAttribute(FlowAttribute key, std::string value);
	//! addAttribute key is string
	bool addAttribute(std::string key, std::string value);
	//! removeAttribute key is enum
	bool removeAttribute(FlowAttribute key);
	//! removeAttribute key is string
	bool removeAttribute(std::string key);
	//! updateAttribute key is enum
	bool updateAttribute(FlowAttribute key, std::string value);
	//! updateAttribute key is string
	bool updateAttribute(std::string key, std::string value);
	//! getAttribute key is enum
	bool getAttribute(FlowAttribute key, std::string &value);
	//! getAttribute key is string
	bool getAttribute(std::string key, std::string &value);
	//! setAttribute, if attribute already there, update it, else, add it
	void setAttribute(std::string key, std::string value) {
		_attributes[key] = value;
	}
	//! Get the UUID as string
	std::string getUUIDStr() {
		return _uuidStr;
	}
	//! Get Attributes
	std::map<std::string, std::string> getAttributes() {
		return _attributes;
	}
	//! Check whether it is still being penalized
	bool isPenalized() {
		return (_penaltyExpirationMs > 0 ? _penaltyExpirationMs > getTimeMillis() : false);
	}
	//! Get Size
	uint64_t getSize() {
		return _size;
	}
	// ! Get Offset
	uint64_t getOffset() {
		return _offset;
	}
	// ! Get Entry Date
	uint64_t getEntryDate() {
		return _entryDate;
	}
	// ! Get Lineage Start Date
	uint64_t getlineageStartDate() {
		return _lineageStartDate;
	}
	// ! Set Original connection
	void setOriginalConnection (Connection *connection) {
		_orginalConnection = connection;
	}
	//! Get Original connection
	Connection * getOriginalConnection() {
		return _orginalConnection;
	}
	//! Get Resource Claim
	ResourceClaim *getResourceClaim() {
		return _claim;
	}
	//! Get lineageIdentifiers
	std::set<std::string> getlineageIdentifiers()
	{
		return _lineageIdentifiers;
	}
	//! Check whether it is stored to DB already
	bool isStoredToRepository()
	{
		return _isStoredToRepo;
	}
	void setStoredToRepository(bool value)
	{
		_isStoredToRepo = value;
	}

protected:

	//! Date at which the flow file entered the flow
	uint64_t _entryDate;
	//! Date at which the origin of this flow file entered the flow
	uint64_t _lineageStartDate;
	//! Date at which the flow file was queued
	uint64_t _lastQueueDate;
	//! Size in bytes of the data corresponding to this flow file
	uint64_t _size;
	//! A global unique identifier
	uuid_t _uuid;
	//! A local unique identifier
	uint64_t _id;
	//! Offset to the content
	uint64_t _offset;
	//! Penalty expiration
	uint64_t _penaltyExpirationMs;
	//! Attributes key/values pairs for the flow record
	std::map<std::string, std::string> _attributes;
	//! Pointer to the associated content resource claim
	ResourceClaim *_claim;
	//! UUID string
	std::string _uuidStr;
	//! UUID string for all parents
	std::set<std::string> _lineageIdentifiers;
	//! whether it is stored to DB
	bool _isStoredToRepo;
	//! duplicate the original flow file
	void duplicate(FlowFileRecord *original);

private:

	//! Local flow sequence ID
	static std::atomic<uint64_t> _localFlowSeqNumber;
	//! Mark for deletion
	bool _markedDelete;
	//! Connection queue that this flow file will be transfer or current in
	Connection *_connection;
	//! Orginal connection queue that this flow file was dequeued from
	Connection *_orginalConnection;
	//! Logger
	std::shared_ptr<Logger> logger_;
	//! Snapshot flow record for session rollback
	bool _snapshot;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	FlowFileRecord(const FlowFileRecord &parent);
	FlowFileRecord &operator=(const FlowFileRecord &parent);

};

#endif
