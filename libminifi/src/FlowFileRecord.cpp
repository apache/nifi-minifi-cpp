/**
 * @file FlowFileRecord.cpp
 * Flow file record class implementation 
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
#include <vector>
#include <queue>
#include <map>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstdio>

#include "FlowFileRecord.h"
#include "Relationship.h"
#include "Logger.h"

std::atomic<uint64_t> FlowFileRecord::_localFlowSeqNumber(0);

FlowFileRecord::FlowFileRecord(std::map<std::string, std::string> attributes, ResourceClaim *claim)
: _size(0),
  _id(_localFlowSeqNumber.load()),
  _offset(0),
  _penaltyExpirationMs(0),
  _claim(claim),
  _markedDelete(false),
  _connection(NULL),
  _orginalConnection(NULL)
{
	_entryDate = getTimeMillis();
	_lineageStartDate = _entryDate;

	char uuidStr[37];

	// Generate the global UUID for the flow record
	uuid_generate(_uuid);
	// Increase the local ID for the flow record
	++_localFlowSeqNumber;
	uuid_unparse_lower(_uuid, uuidStr);
	_uuidStr = uuidStr;

	// Populate the default attributes
    addAttribute(FILENAME, std::to_string(getTimeNano()));
    addAttribute(PATH, DEFAULT_FLOWFILE_PATH);
    addAttribute(UUID, uuidStr);
	// Populate the attributes from the input
    std::map<std::string, std::string>::iterator it;
    for (it = attributes.begin(); it!= attributes.end(); it++)
    {
    	addAttribute(it->first, it->second);
    }

    _snapshot = false;

	if (_claim)
		// Increase the flow file record owned count for the resource claim
		_claim->increaseFlowFileRecordOwnedCount();
	_logger = Logger::getLogger();
}

FlowFileRecord::~FlowFileRecord()
{
	if (!_snapshot)
		_logger->log_debug("Delete FlowFile UUID %s", _uuidStr.c_str());
	else
		_logger->log_debug("Delete SnapShot FlowFile UUID %s", _uuidStr.c_str());
	if (_claim)
	{
		// Decrease the flow file record owned count for the resource claim
		_claim->decreaseFlowFileRecordOwnedCount();
		if (_claim->getFlowFileRecordOwnedCount() <= 0)
		{
			_logger->log_debug("Delete Resource Claim %s", _claim->getContentFullPath().c_str());
			std::remove(_claim->getContentFullPath().c_str());
			delete _claim;
		}
	}
}

bool FlowFileRecord::addAttribute(FlowAttribute key, std::string value)
{
	const char *keyStr = FlowAttributeKey(key);
	if (keyStr)
	{
		std::string keyString = keyStr;
		return addAttribute(keyString, value);
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::addAttribute(std::string key, std::string value)
{
	std::map<std::string, std::string>::iterator it = _attributes.find(key);
	if (it != _attributes.end())
	{
		// attribute already there in the map
		return false;
	}
	else
	{
		_attributes[key] = value;
		return true;
	}
}

bool FlowFileRecord::removeAttribute(FlowAttribute key)
{
	const char *keyStr = FlowAttributeKey(key);
	if (keyStr)
	{
		std::string keyString = keyStr;
		return removeAttribute(keyString);
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::removeAttribute(std::string key)
{
	std::map<std::string, std::string>::iterator it = _attributes.find(key);
	if (it != _attributes.end())
	{
		_attributes.erase(key);
		return true;
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::updateAttribute(FlowAttribute key, std::string value)
{
	const char *keyStr = FlowAttributeKey(key);
	if (keyStr)
	{
		std::string keyString = keyStr;
		return updateAttribute(keyString, value);
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::updateAttribute(std::string key, std::string value)
{
	std::map<std::string, std::string>::iterator it = _attributes.find(key);
	if (it != _attributes.end())
	{
		_attributes[key] = value;
		return true;
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::getAttribute(FlowAttribute key, std::string &value)
{
	const char *keyStr = FlowAttributeKey(key);
	if (keyStr)
	{
		std::string keyString = keyStr;
		return getAttribute(keyString, value);
	}
	else
	{
		return false;
	}
}

bool FlowFileRecord::getAttribute(std::string key, std::string &value)
{
	std::map<std::string, std::string>::iterator it = _attributes.find(key);
	if (it != _attributes.end())
	{
		value = it->second;
		return true;
	}
	else
	{
		return false;
	}
}

void FlowFileRecord::duplicate(FlowFileRecord *original)
{
	uuid_copy(this->_uuid, original->_uuid);
	this->_attributes = original->_attributes;
	this->_entryDate = original->_entryDate;
	this->_id = original->_id;
	this->_lastQueueDate = original->_lastQueueDate;
	this->_lineageStartDate = original->_lineageStartDate;
	this->_offset = original->_offset;
	this->_penaltyExpirationMs = original->_penaltyExpirationMs;
	this->_size = original->_size;
	this->_lineageIdentifiers = original->_lineageIdentifiers;
	this->_orginalConnection = original->_orginalConnection;
	this->_uuidStr = original->_uuidStr;
	this->_connection = original->_connection;
	this->_markedDelete = original->_markedDelete;

	this->_claim = original->_claim;
	if (this->_claim)
		this->_claim->increaseFlowFileRecordOwnedCount();

	this->_snapshot = true;
}

