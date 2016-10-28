/**
 * @file Provenance.cpp
 * Provenance implemenatation 
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
#include "Provenance.h"
#include "Relationship.h"
#include "Logger.h"
#include "FlowController.h"

int ProvenanceEventRecord::readUTF(std::string &str, bool widen)
{
    uint16_t utflen;
    int ret;

    if (!widen)
    {
    	ret = read(utflen);
    	if (ret <= 0)
    		return ret;
    }
    else
    {
    	uint32_t len;
       	ret = read(len);
        if (ret <= 0)
        	return ret;
        utflen = len;
    }

    uint8_t *bytearr = NULL;
    char *chararr = NULL;
    bytearr = new uint8_t[utflen];
    chararr = new char[utflen];
    memset(chararr, 0, utflen);

    int c, char2, char3;
    int count = 0;
    int chararr_count=0;

    ret = read(bytearr, utflen);
    if (ret <= 0)
    {
    	delete[] bytearr;
    	delete[] chararr;
    	if (ret == 0)
    	{
    	 if (!widen)
    	    	return (2 + utflen);
    	    else
    	    	return (4 + utflen);
    	}
    	else
    		return ret;
    }

    while (count < utflen) {
        c = (int) bytearr[count] & 0xff;
        if (c > 127) break;
        count++;
        chararr[chararr_count++]=(char)c;
    }

    while (count < utflen) {
        c = (int) bytearr[count] & 0xff;
        switch (c >> 4) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                /* 0xxxxxxx*/
                count++;
                chararr[chararr_count++]=(char)c;
                break;
            case 12: case 13:
                /* 110x xxxx   10xx xxxx*/
                count += 2;
                if (count > utflen)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                char2 = (int) bytearr[count-1];
                if ((char2 & 0xC0) != 0x80)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                chararr[chararr_count++]=(char)(((c & 0x1F) << 6) |
                                                (char2 & 0x3F));
                break;
            case 14:
                /* 1110 xxxx  10xx xxxx  10xx xxxx */
                count += 3;
                if (count > utflen)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                char2 = (int) bytearr[count-2];
                char3 = (int) bytearr[count-1];
                if (((char2 & 0xC0) != 0x80) || ((char3 & 0xC0) != 0x80))
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                chararr[chararr_count++]=(char)(((c     & 0x0F) << 12) |
                                                ((char2 & 0x3F) << 6)  |
                                                ((char3 & 0x3F) << 0));
                break;
            default:
            	delete[] bytearr;
            	delete[] chararr;
            	return -1;
        }
    }
    // The number of chars produced may be less than utflen
    std::string value(chararr, chararr_count);
    str = value;
    delete[] bytearr;
    delete[] chararr;
    if (!widen)
    	return (2 + utflen);
    else
    	return (4 + utflen);
}

int ProvenanceEventRecord::writeUTF(std::string str, bool widen)
{
	int strlen = str.length();
	int utflen = 0;
	int c, count = 0;

	/* use charAt instead of copying String to char array */
	for (int i = 0; i < strlen; i++) {
		c = str.at(i);
		if ((c >= 0x0001) && (c <= 0x007F)) {
			utflen++;
		} else if (c > 0x07FF) {
			utflen += 3;
		} else {
			utflen += 2;
		}
	}

	if (utflen > 65535)
		return -1;

	uint8_t *bytearr = NULL;
	if (!widen)
	{
		bytearr = new uint8_t[utflen+2];
		bytearr[count++] = (uint8_t) ((utflen >> 8) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 0) & 0xFF);
	}
	else
	{
		bytearr = new uint8_t[utflen+4];
		bytearr[count++] = (uint8_t) ((utflen >> 24) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 16) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 8) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 0) & 0xFF);
	}

	int i=0;
	for (i=0; i<strlen; i++) {
		c = str.at(i);
		if (!((c >= 0x0001) && (c <= 0x007F))) break;
		bytearr[count++] = (uint8_t) c;
	}

	for (;i < strlen; i++){
		c = str.at(i);
		if ((c >= 0x0001) && (c <= 0x007F)) {
			bytearr[count++] = (uint8_t) c;
		} else if (c > 0x07FF) {
			bytearr[count++] = (uint8_t) (0xE0 | ((c >> 12) & 0x0F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  6) & 0x3F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  0) & 0x3F));
		} else {
			bytearr[count++] = (uint8_t) (0xC0 | ((c >>  6) & 0x1F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  0) & 0x3F));
		}
	}
	int ret;
	if (!widen)
	{
		ret = writeData(bytearr, utflen+2);
	}
	else
	{
		ret = writeData(bytearr, utflen+4);
	}
	delete[] bytearr;
	return ret;
}

//! DeSerialize
bool ProvenanceEventRecord::DeSerialize(ProvenanceRepository *repo, std::string key)
{
	std::string value;
	bool ret;

	ret = repo->Get(key, value);

	if (!ret)
	{
		_logger->log_error("NiFi Provenance Store event %s can not found", key.c_str());
		return false;
	}
	else
		_logger->log_debug("NiFi Provenance Read event %s length %d", key.c_str(), value.length());

	ret = DeSerialize((unsigned char *) value.data(), value.length());

	if (ret)
	{
		_logger->log_debug("NiFi Provenance retrieve event %s size %d eventType %d success", _eventIdStr.c_str(), _serializeBufSize, _eventType);
	}
	else
	{
		_logger->log_debug("NiFi Provenance retrieve event %s size %d eventType %d fail", _eventIdStr.c_str(), _serializeBufSize, _eventType);
	}

	return ret;
}

bool ProvenanceEventRecord::Serialize(ProvenanceRepository *repo)
{
	if (_serializedBuf)
		// Serialize in progress
		return false;
	_serializedBuf = NULL;
	_serializeBufSize = 0;
	_maxSerializeBufSize = 0;
	_serializedBuf = new uint8_t[PROVENANCE_EVENT_RECORD_SEG_SIZE];
	if (!_serializedBuf)
		return false;
	_maxSerializeBufSize = PROVENANCE_EVENT_RECORD_SEG_SIZE;

	int ret;

	ret = writeUTF(this->_eventIdStr);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	uint32_t eventType = this->_eventType;
	ret = write(eventType);
	if (ret != 4)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_eventTime);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_entryDate);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_eventDuration);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_lineageStartDate);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = writeUTF(this->_componentId);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = writeUTF(this->_componentType);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = writeUTF(this->_uuid);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = writeUTF(this->_details);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	// write flow attributes
	uint32_t numAttributes = this->_attributes.size();
	ret = write(numAttributes);
	if (ret != 4)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	std::map<std::string, std::string>::iterator itAttribute;
	for (itAttribute = this->_attributes.begin(); itAttribute!= this->_attributes.end(); itAttribute++)
	{
		ret = writeUTF(itAttribute->first, true);
		if (ret <= 0)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
		ret = writeUTF(itAttribute->second, true);
		if (ret <= 0)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
	}

	ret = writeUTF(this->_contentFullPath);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_size);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = write(this->_offset);
	if (ret != 8)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	ret = writeUTF(this->_sourceQueueIdentifier);
	if (ret <= 0)
	{
		delete[] _serializedBuf;
		_serializedBuf = NULL;
		return false;
	}

	if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN)
	{
		// write UUIDs
		uint32_t number = this->_parentUuids.size();
		ret = write(number);
		if (ret != 4)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
		std::vector<std::string>::iterator it;
		for (it = this->_parentUuids.begin(); it!= this->_parentUuids.end(); it++)
		{
			std::string parentUUID = *it;
			ret = writeUTF(parentUUID);
			if (ret <= 0)
			{
				delete[] _serializedBuf;
				_serializedBuf = NULL;
				return false;
			}
		}
		number = this->_childrenUuids.size();
		ret = write(number);
		if (ret != 4)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
		for (it = this->_childrenUuids.begin(); it!= this->_childrenUuids.end(); it++)
		{
			std::string childUUID = *it;
			ret = writeUTF(childUUID);
			if (ret <= 0)
			{
				delete[] _serializedBuf;
				_serializedBuf = NULL;
				return false;
			}
		}
	}
	else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH)
	{
		ret = writeUTF(this->_transitUri);
		if (ret <= 0)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
	}
	else if (this->_eventType == ProvenanceEventRecord::RECEIVE)
	{
		ret = writeUTF(this->_transitUri);
		if (ret <= 0)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
		ret = writeUTF(this->_sourceSystemFlowFileIdentifier);
		if (ret <= 0)
		{
			delete[] _serializedBuf;
			_serializedBuf = NULL;
			return false;
		}
	}

	// Persistent to the DB
	if (repo->Put(_eventIdStr, _serializedBuf, _serializeBufSize))
	{
		_logger->log_debug("NiFi Provenance Store event %s size %d success", _eventIdStr.c_str(), _serializeBufSize);
	}
	else
	{
		_logger->log_error("NiFi Provenance Store event %s size %d fail", _eventIdStr.c_str(), _serializeBufSize);
	}

	// cleanup
	delete[] (_serializedBuf);
	_serializedBuf = NULL;
	_serializeBufSize = 0;

	return true;
}

bool ProvenanceEventRecord::DeSerialize(uint8_t *buffer, int bufferSize)
{
	_serializedBuf = buffer;
	_serializeBufSize = 0;
	_maxSerializeBufSize = bufferSize;

	int ret;

	ret = readUTF(this->_eventIdStr);
	if (ret <= 0)
	{
		return false;
	}

	uint32_t eventType;
	ret = read(eventType);
	if (ret != 4)
	{
		return false;
	}
	this->_eventType = (ProvenanceEventRecord::ProvenanceEventType) eventType;

	ret = read(this->_eventTime);
	if (ret != 8)
	{
		return false;
	}

	ret = read(this->_entryDate);
	if (ret != 8)
	{
		return false;
	}

	ret = read(this->_eventDuration);
	if (ret != 8)
	{
		return false;
	}

	ret = read(this->_lineageStartDate);
	if (ret != 8)
	{
		return false;
	}

	ret = readUTF(this->_componentId);
	if (ret <= 0)
	{
		return false;
	}

	ret = readUTF(this->_componentType);
	if (ret <= 0)
	{
		return false;
	}

	ret = readUTF(this->_uuid);
	if (ret <= 0)
	{
		return false;
	}

	ret = readUTF(this->_details);
	if (ret <= 0)
	{
		return false;
	}

	// read flow attributes
	uint32_t numAttributes = 0;
	ret = read(numAttributes);
	if (ret != 4)
	{
		return false;
	}

	for (uint32_t i = 0; i < numAttributes; i++)
	{
		std::string key;
		ret = readUTF(key, true);
		if (ret <= 0)
		{
			return false;
		}
		std::string value;
		ret = readUTF(value, true);
		if (ret <= 0)
		{
			return false;
		}
		this->_attributes[key] = value;
	}

	ret = readUTF(this->_contentFullPath);
	if (ret <= 0)
	{
		return false;
	}

	ret = read(this->_size);
	if (ret != 8)
	{
		return false;
	}

	ret = read(this->_offset);
	if (ret != 8)
	{
		return false;
	}

	ret = readUTF(this->_sourceQueueIdentifier);
	if (ret <= 0)
	{
		return false;
	}

	if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN)
	{
		// read UUIDs
		uint32_t number = 0;
		ret = read(number);
		if (ret != 4)
		{
			return false;
		}
		for (uint32_t i = 0; i < number; i++)
		{
			std::string parentUUID;
			ret = readUTF(parentUUID);
			if (ret <= 0)
			{
				return false;
			}
			this->addParentUuid(parentUUID);
		}
		number = 0;
		ret = read(number);
		if (ret != 4)
		{
			return false;
		}
		for (uint32_t i = 0; i < number; i++)
		{
			std::string childUUID;
			ret = readUTF(childUUID);
			if (ret <= 0)
			{
				return false;
			}
			this->addChildUuid(childUUID);
		}
	}
	else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH)
	{
		ret = readUTF(this->_transitUri);
		if (ret <= 0)
		{
			return false;
		}
	}
	else if (this->_eventType == ProvenanceEventRecord::RECEIVE)
	{
		ret = readUTF(this->_transitUri);
		if (ret <= 0)
		{
			return false;
		}
		ret = readUTF(this->_sourceSystemFlowFileIdentifier);
		if (ret <= 0)
		{
			return false;
		}
	}

	return true;
}

void ProvenanceReporter::commit()
{
	for (std::set<ProvenanceEventRecord*>::iterator it = _events.begin(); it != _events.end(); ++it)
	{
		ProvenanceEventRecord *event = (ProvenanceEventRecord *) (*it);
		if (!FlowController::getFlowController()->getProvenanceRepository()->isFull())
			event->Serialize(FlowController::getFlowController()->getProvenanceRepository());
		else
			_logger->log_debug("Provenance Repository is full");
	}
}

void ProvenanceReporter::create(FlowFileRecord *flow, std::string detail)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::CREATE, flow);

	if (event)
	{
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::route(FlowFileRecord *flow, Relationship relation, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::ROUTE, flow);

	if (event)
	{
		event->setDetails(detail);
		event->setRelationship(relation.getName());
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::modifyAttributes(FlowFileRecord *flow, std::string detail)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::ATTRIBUTES_MODIFIED, flow);

	if (event)
	{
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::modifyContent(FlowFileRecord *flow, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::CONTENT_MODIFIED, flow);

	if (event)
	{
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::clone(FlowFileRecord *parent, FlowFileRecord *child)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::CLONE, parent);

	if (event)
	{
		event->addChildFlowFile(child);
		event->addParentFlowFile(parent);
		add(event);
	}
}

void ProvenanceReporter::join(std::vector<FlowFileRecord *> parents, FlowFileRecord *child, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::JOIN, child);

	if (event)
	{
		event->addChildFlowFile(child);
		std::vector<FlowFileRecord *>::iterator it;
		for (it = parents.begin(); it!= parents.end(); it++)
		{
			FlowFileRecord *record = *it;
			event->addParentFlowFile(record);
		}
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::fork(std::vector<FlowFileRecord *> child, FlowFileRecord *parent, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::FORK, parent);

	if (event)
	{
		event->addParentFlowFile(parent);
		std::vector<FlowFileRecord *>::iterator it;
		for (it = child.begin(); it!= child.end(); it++)
		{
			FlowFileRecord *record = *it;
			event->addChildFlowFile(record);
		}
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::expire(FlowFileRecord *flow, std::string detail)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::EXPIRE, flow);

	if (event)
	{
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::drop(FlowFileRecord *flow, std::string reason)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::DROP, flow);

	if (event)
	{
		std::string dropReason = "Discard reason: " + reason;
		event->setDetails(dropReason);
		add(event);
	}
}

void ProvenanceReporter::send(FlowFileRecord *flow, std::string transitUri, std::string detail, uint64_t processingDuration, bool force)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::SEND, flow);

	if (event)
	{
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		if (!force)
		{
			add(event);
		}
		else
		{
			if (!FlowController::getFlowController()->getProvenanceRepository()->isFull())
				event->Serialize(FlowController::getFlowController()->getProvenanceRepository());
			delete event;
		}
	}
}

void ProvenanceReporter::receive(FlowFileRecord *flow, std::string transitUri, std::string sourceSystemFlowFileIdentifier, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::RECEIVE, flow);

	if (event)
	{
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		event->setSourceSystemFlowFileIdentifier(sourceSystemFlowFileIdentifier);
		add(event);
	}
}

void ProvenanceReporter::fetch(FlowFileRecord *flow, std::string transitUri, std::string detail, uint64_t processingDuration)
{
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::FETCH, flow);

	if (event)
	{
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

uint64_t ProvenanceRepository::_repoSize = 0;

void ProvenanceRepository::start()
{
	if (this->_purgePeriod <= 0)
		return;
	if (_running)
		return;
	_running = true;
	_logger->log_info("ProvenanceRepository Monitor Thread Start");
	_thread = new std::thread(run, this);
	_thread->detach();
}

void ProvenanceRepository::stop()
{
	if (!_running)
		return;
	_running = false;
	_logger->log_info("ProvenanceRepository Monitor Thread Stop");
}

void ProvenanceRepository::run(ProvenanceRepository *repo)
{
	// threshold for purge
	uint64_t purgeThreshold = repo->_maxPartitionBytes*3/4;
	while (repo->_running)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(repo->_purgePeriod));
		uint64_t curTime = getTimeMillis();
		uint64_t size = repo->repoSize();
		if (size >= purgeThreshold)
		{
			std::vector<std::string> purgeList;
			leveldb::Iterator* it = repo->_db->NewIterator(leveldb::ReadOptions());
			for (it->SeekToFirst(); it->Valid(); it->Next())
			{
				ProvenanceEventRecord eventRead;
				std::string key = it->key().ToString();
				if (eventRead.DeSerialize((uint8_t *)it->value().data(), (int) it->value().size()))
				{
					if ((curTime - eventRead.getEventTime()) > repo->_maxPartitionMillis)
						purgeList.push_back(key);
				}
				else
				{
					repo->_logger->log_debug("NiFi Provenance retrieve event %s fail", key.c_str());
					purgeList.push_back(key);
				}
			}
			delete it;
			std::vector<std::string>::iterator itPurge;
			for (itPurge = purgeList.begin(); itPurge!= purgeList.end(); itPurge++)
			{
				std::string eventId = *itPurge;
				repo->_logger->log_info("ProvenanceRepository Repo Purge %s", eventId.c_str());
				repo->Delete(eventId);
			}
		}
		if (size > repo->_maxPartitionBytes)
			repo->_repoFull = true;
		else
			repo->_repoFull = false;
	}
	return;
}




