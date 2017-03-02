/**
 * @file FlowFileRepository.cpp
 * FlowFile implemenatation 
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
#include <cstdint>
#include <vector>
#include <arpa/inet.h>
#include "io/DataStream.h"
#include "io/Serializable.h"
#include "FlowFileRecord.h"
#include "Relationship.h"
#include "Logger.h"
#include "FlowController.h"
#include "FlowFileRepository.h"

//! DeSerialize
bool FlowFileEventRecord::DeSerialize(FlowFileRepository *repo,
		std::string key) {
	std::string value;
	bool ret;

	ret = repo->Get(key, value);

	if (!ret) {
		logger_->log_error("NiFi FlowFile Store event %s can not found",
				key.c_str());
		return false;
	} else
		logger_->log_debug("NiFi FlowFile Read event %s length %d",
				key.c_str(), value.length());


	DataStream stream((const uint8_t*)value.data(),value.length());

	ret = DeSerialize(stream);

	if (ret) {
		logger_->log_debug(
				"NiFi FlowFile retrieve uuid %s size %d connection %s success",
				_uuid.c_str(), stream.getSize(), _uuidConnection.c_str());
	} else {
		logger_->log_debug(
				"NiFi FlowFile retrieve uuid %s size %d connection %d fail",
				_uuid.c_str(), stream.getSize(), _uuidConnection.c_str());
	}

	return ret;
}

bool FlowFileEventRecord::Serialize(FlowFileRepository *repo) {

	DataStream outStream;

	int ret;

	ret = write(this->_eventTime,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = write(this->_entryDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = write(this->_lineageStartDate,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = writeUTF(this->_uuid,&outStream);
	if (ret <= 0) {

		return false;
	}

	ret = writeUTF(this->_uuidConnection,&outStream);
	if (ret <= 0) {

		return false;
	}

	// write flow attributes
	uint32_t numAttributes = this->_attributes.size();
	ret = write(numAttributes,&outStream);
	if (ret != 4) {

		return false;
	}

	for (auto itAttribute : _attributes) {
		ret = writeUTF(itAttribute.first,&outStream, true);
		if (ret <= 0) {

			return false;
		}
		ret = writeUTF(itAttribute.second,&outStream, true);
		if (ret <= 0) {

			return false;
		}
	}

	ret = writeUTF(this->_contentFullPath,&outStream);
	if (ret <= 0) {

		return false;
	}

	ret = write(this->_size,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = write(this->_offset,&outStream);
	if (ret != 8) {

		return false;
	}

	// Persistent to the DB

	if (repo->Put(_uuid, const_cast<uint8_t*>(outStream.getBuffer()), outStream.getSize())) {
		logger_->log_debug("NiFi FlowFile Store event %s size %d success",
				_uuid.c_str(), outStream.getSize());
		return true;
	} else {
		logger_->log_error("NiFi FlowFile Store event %s size %d fail",
				_uuid.c_str(), outStream.getSize());
		return false;
	}

	// cleanup

	return true;
}

bool FlowFileEventRecord::DeSerialize(const uint8_t *buffer, const int bufferSize) {

	int ret;

	DataStream outStream(buffer,bufferSize);

	ret = read(this->_eventTime,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_entryDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_lineageStartDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = readUTF(this->_uuid,&outStream);
	if (ret <= 0) {
		return false;
	}

	ret = readUTF(this->_uuidConnection,&outStream);
	if (ret <= 0) {
		return false;
	}

	// read flow attributes
	uint32_t numAttributes = 0;
	ret = read(numAttributes,&outStream);
	if (ret != 4) {
		return false;
	}

	for (uint32_t i = 0; i < numAttributes; i++) {
		std::string key;
		ret = readUTF(key,&outStream, true);
		if (ret <= 0) {
			return false;
		}
		std::string value;
		ret = readUTF(value,&outStream, true);
		if (ret <= 0) {
			return false;
		}
		this->_attributes[key] = value;
	}

	ret = readUTF(this->_contentFullPath,&outStream);
	if (ret <= 0) {
		return false;
	}

	ret = read(this->_size,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_offset,&outStream);
	if (ret != 8) {
		return false;
	}

	return true;
}

void FlowFileRepository::loadFlowFileToConnections(std::map<std::string, Connection *> *connectionMap)
{
	if (!_enable)
		return;

	std::vector<std::string> purgeList;
	leveldb::Iterator* it = _db->NewIterator(
						leveldb::ReadOptions());

	for (it->SeekToFirst(); it->Valid(); it->Next())
	{
		FlowFileEventRecord eventRead;
		std::string key = it->key().ToString();
		if (eventRead.DeSerialize((uint8_t *) it->value().data(),
				(int) it->value().size()))
		{
			auto search = connectionMap->find(eventRead.getConnectionUuid());
			if (search != connectionMap->end())
			{
				// we find the connection for the persistent flowfile, create the flowfile and enqueue that
				FlowFileRecord *record = new FlowFileRecord(&eventRead);
				// set store to repo to true so that we do need to persistent again in enqueue
				record->setStoredToRepository(true);
				search->second->put(record);
			}
			else
			{
				if (eventRead.getContentFullPath().length() > 0)
				{
					std::remove(eventRead.getContentFullPath().c_str());
				}
				purgeList.push_back(key);
			}
		}
		else
		{
			purgeList.push_back(key);
		}
	}

	delete it;
	std::vector<std::string>::iterator itPurge;
	for (itPurge = purgeList.begin(); itPurge != purgeList.end();
						itPurge++)
	{
		std::string eventId = *itPurge;
		logger_->log_info("Repository Repo %s Purge %s",
										RepositoryTypeStr[_type],
										eventId.c_str());
		Delete(eventId);
	}

	return;
}

