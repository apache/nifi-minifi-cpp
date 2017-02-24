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
#include <cstdint>
#include <vector>
#include <arpa/inet.h>
#include "io/DataStream.h"
#include "io/Serializable.h"
#include "Provenance.h"

#include "Logger.h"
#include "Relationship.h"
#include "FlowController.h"

//! DeSerialize
bool ProvenanceEventRecord::DeSerialize(ProvenanceRepository *repo,
		std::string key) {
	std::string value;
	bool ret;

	ret = repo->Get(key, value);

	if (!ret) {
		logger_->log_error("NiFi Provenance Store event %s can not found",
				key.c_str());
		return false;
	} else
		logger_->log_debug("NiFi Provenance Read event %s length %d",
				key.c_str(), value.length());


	DataStream stream((const uint8_t*)value.data(),value.length());

	ret = DeSerialize(stream);

	if (ret) {
		logger_->log_debug(
				"NiFi Provenance retrieve event %s size %d eventType %d success",
				_eventIdStr.c_str(), stream.getSize(), _eventType);
	} else {
		logger_->log_debug(
				"NiFi Provenance retrieve event %s size %d eventType %d fail",
				_eventIdStr.c_str(), stream.getSize(), _eventType);
	}

	return ret;
}

bool ProvenanceEventRecord::Serialize(ProvenanceRepository *repo) {

	DataStream outStream;

	int ret;

	ret = writeUTF(this->_eventIdStr,&outStream);
	if (ret <= 0) {

		return false;
	}

	uint32_t eventType = this->_eventType;
	ret = write(eventType,&outStream);
	if (ret != 4) {

		return false;
	}

	ret = write(this->_eventTime,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = write(this->_entryDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = write(this->_eventDuration,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = write(this->_lineageStartDate,&outStream);
	if (ret != 8) {

		return false;
	}

	ret = writeUTF(this->_componentId,&outStream);
	if (ret <= 0) {

		return false;
	}

	ret = writeUTF(this->_componentType,&outStream);
	if (ret <= 0) {

		return false;
	}

	ret = writeUTF(this->_uuid,&outStream);
	if (ret <= 0) {

		return false;
	}

	ret = writeUTF(this->_details,&outStream);
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

	ret = writeUTF(this->_sourceQueueIdentifier,&outStream);
	if (ret <= 0) {

		return false;
	}

	if (this->_eventType == ProvenanceEventRecord::FORK
			|| this->_eventType == ProvenanceEventRecord::CLONE
			|| this->_eventType == ProvenanceEventRecord::JOIN) {
		// write UUIDs
		uint32_t number = this->_parentUuids.size();
		ret = write(number,&outStream);
		if (ret != 4) {

			return false;
		}
		for (auto parentUUID : _parentUuids) {
			ret = writeUTF(parentUUID,&outStream);
			if (ret <= 0) {

				return false;
			}
		}
		number = this->_childrenUuids.size();
		ret = write(number,&outStream);
		if (ret != 4) {
			return false;
		}
		for (auto childUUID : _childrenUuids) {
			ret = writeUTF(childUUID,&outStream);
			if (ret <= 0) {

				return false;
			}
		}
	} else if (this->_eventType == ProvenanceEventRecord::SEND
			|| this->_eventType == ProvenanceEventRecord::FETCH) {
		ret = writeUTF(this->_transitUri,&outStream);
		if (ret <= 0) {

			return false;
		}
	} else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
		ret = writeUTF(this->_transitUri,&outStream);
		if (ret <= 0) {

			return false;
		}
		ret = writeUTF(this->_sourceSystemFlowFileIdentifier,&outStream);
		if (ret <= 0) {

			return false;
		}
	}

	// Persistent to the DB

	if (repo->Put(_eventIdStr, const_cast<uint8_t*>(outStream.getBuffer()), outStream.getSize())) {
		logger_->log_debug("NiFi Provenance Store event %s size %d success",
				_eventIdStr.c_str(), outStream.getSize());
	} else {
		logger_->log_error("NiFi Provenance Store event %s size %d fail",
				_eventIdStr.c_str(), outStream.getSize());
	}

	// cleanup

	return true;
}

bool ProvenanceEventRecord::DeSerialize(const uint8_t *buffer, const int bufferSize) {

	int ret;

	DataStream outStream(buffer,bufferSize);

	ret = readUTF(this->_eventIdStr,&outStream);

	if (ret <= 0) {
		return false;
	}

	uint32_t eventType;
	ret = read(eventType,&outStream);
	if (ret != 4) {
		return false;
	}
	this->_eventType = (ProvenanceEventRecord::ProvenanceEventType) eventType;

	ret = read(this->_eventTime,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_entryDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_eventDuration,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = read(this->_lineageStartDate,&outStream);
	if (ret != 8) {
		return false;
	}

	ret = readUTF(this->_componentId,&outStream);
	if (ret <= 0) {
		return false;
	}

	ret = readUTF(this->_componentType,&outStream);
	if (ret <= 0) {
		return false;
	}

	ret = readUTF(this->_uuid,&outStream);
	if (ret <= 0) {
		return false;
	}

	ret = readUTF(this->_details,&outStream);

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

	ret = readUTF(this->_sourceQueueIdentifier,&outStream);
	if (ret <= 0) {
		return false;
	}

	if (this->_eventType == ProvenanceEventRecord::FORK
			|| this->_eventType == ProvenanceEventRecord::CLONE
			|| this->_eventType == ProvenanceEventRecord::JOIN) {
		// read UUIDs
		uint32_t number = 0;
		ret = read(number,&outStream);
		if (ret != 4) {
			return false;
		}


		for (uint32_t i = 0; i < number; i++) {
			std::string parentUUID;
			ret = readUTF(parentUUID,&outStream);
			if (ret <= 0) {
				return false;
			}
			this->addParentUuid(parentUUID);
		}
		number = 0;
		ret = read(number,&outStream);
		if (ret != 4) {
			return false;
		}
		for (uint32_t i = 0; i < number; i++) {
			std::string childUUID;
			ret = readUTF(childUUID,&outStream);
			if (ret <= 0) {
				return false;
			}
			this->addChildUuid(childUUID);
		}
	} else if (this->_eventType == ProvenanceEventRecord::SEND
			|| this->_eventType == ProvenanceEventRecord::FETCH) {
		ret = readUTF(this->_transitUri,&outStream);
		if (ret <= 0) {
			return false;
		}
	} else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
		ret = readUTF(this->_transitUri,&outStream);
		if (ret <= 0) {
			return false;
		}
		ret = readUTF(this->_sourceSystemFlowFileIdentifier,&outStream);
		if (ret <= 0) {
			return false;
		}
	}

	return true;
}

void ProvenanceReporter::commit() {
	for (auto event : _events) {
		if (!FlowControllerFactory::getFlowController()->getProvenanceRepository()->isFull()) {
			event->Serialize(
					FlowControllerFactory::getFlowController()->getProvenanceRepository());
		} else {
			logger_->log_debug("Provenance Repository is full");
		}
	}
}

void ProvenanceReporter::create(FlowFileRecord *flow, std::string detail) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::CREATE,
			flow);

	if (event) {
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::route(FlowFileRecord *flow, Relationship relation,
		std::string detail, uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::ROUTE, flow);

	if (event) {
		event->setDetails(detail);
		event->setRelationship(relation.getName());
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::modifyAttributes(FlowFileRecord *flow,
		std::string detail) {
	ProvenanceEventRecord *event = allocate(
			ProvenanceEventRecord::ATTRIBUTES_MODIFIED, flow);

	if (event) {
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::modifyContent(FlowFileRecord *flow, std::string detail,
		uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(
			ProvenanceEventRecord::CONTENT_MODIFIED, flow);

	if (event) {
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::clone(FlowFileRecord *parent, FlowFileRecord *child) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::CLONE,
			parent);

	if (event) {
		event->addChildFlowFile(child);
		event->addParentFlowFile(parent);
		add(event);
	}
}

void ProvenanceReporter::join(std::vector<FlowFileRecord *> parents,
		FlowFileRecord *child, std::string detail,
		uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::JOIN, child);

	if (event) {
		event->addChildFlowFile(child);
		std::vector<FlowFileRecord *>::iterator it;
		for (it = parents.begin(); it != parents.end(); it++) {
			FlowFileRecord *record = *it;
			event->addParentFlowFile(record);
		}
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::fork(std::vector<FlowFileRecord *> child,
		FlowFileRecord *parent, std::string detail,
		uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::FORK,
			parent);

	if (event) {
		event->addParentFlowFile(parent);
		std::vector<FlowFileRecord *>::iterator it;
		for (it = child.begin(); it != child.end(); it++) {
			FlowFileRecord *record = *it;
			event->addChildFlowFile(record);
		}
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

void ProvenanceReporter::expire(FlowFileRecord *flow, std::string detail) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::EXPIRE,
			flow);

	if (event) {
		event->setDetails(detail);
		add(event);
	}
}

void ProvenanceReporter::drop(FlowFileRecord *flow, std::string reason) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::DROP, flow);

	if (event) {
		std::string dropReason = "Discard reason: " + reason;
		event->setDetails(dropReason);
		add(event);
	}
}

void ProvenanceReporter::send(FlowFileRecord *flow, std::string transitUri,
		std::string detail, uint64_t processingDuration, bool force) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::SEND, flow);

	if (event) {
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		if (!force) {
			add(event);
		} else {
			if (!FlowControllerFactory::getFlowController()->getProvenanceRepository()->isFull())
				event->Serialize(
						FlowControllerFactory::getFlowController()->getProvenanceRepository());
			delete event;
		}
	}
}

void ProvenanceReporter::receive(FlowFileRecord *flow, std::string transitUri,
		std::string sourceSystemFlowFileIdentifier, std::string detail,
		uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::RECEIVE,
			flow);

	if (event) {
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		event->setSourceSystemFlowFileIdentifier(
				sourceSystemFlowFileIdentifier);
		add(event);
	}
}

void ProvenanceReporter::fetch(FlowFileRecord *flow, std::string transitUri,
		std::string detail, uint64_t processingDuration) {
	ProvenanceEventRecord *event = allocate(ProvenanceEventRecord::FETCH, flow);

	if (event) {
		event->setTransitUri(transitUri);
		event->setDetails(detail);
		event->setEventDuration(processingDuration);
		add(event);
	}
}

uint64_t ProvenanceRepository::_repoSize = 0;

void ProvenanceRepository::start() {
	if (this->_purgePeriod <= 0)
		return;
	if (_running)
		return;
	_running = true;
	logger_->log_info("ProvenanceRepository Monitor Thread Start");
	_thread = new std::thread(run, this);
	_thread->detach();
}

void ProvenanceRepository::stop() {
	if (!_running)
		return;
	_running = false;
	logger_->log_info("ProvenanceRepository Monitor Thread Stop");
}

void ProvenanceRepository::run(ProvenanceRepository *repo) {
	// threshold for purge
	uint64_t purgeThreshold = repo->_maxPartitionBytes * 3 / 4;
	while (repo->_running) {
		std::this_thread::sleep_for(
				std::chrono::milliseconds(repo->_purgePeriod));
		uint64_t curTime = getTimeMillis();
		uint64_t size = repo->repoSize();
		if (size >= purgeThreshold) {
			std::vector<std::string> purgeList;
			leveldb::Iterator* it = repo->_db->NewIterator(
					leveldb::ReadOptions());
			for (it->SeekToFirst(); it->Valid(); it->Next()) {
				ProvenanceEventRecord eventRead;
				std::string key = it->key().ToString();
				if (eventRead.DeSerialize((uint8_t *) it->value().data(),
						(int) it->value().size())) {
					if ((curTime - eventRead.getEventTime())
							> repo->_maxPartitionMillis)
						purgeList.push_back(key);
				} else {
					repo->logger_->log_debug(
							"NiFi Provenance retrieve event %s fail",
							key.c_str());
					purgeList.push_back(key);
				}
			}
			delete it;
			std::vector<std::string>::iterator itPurge;
			for (itPurge = purgeList.begin(); itPurge != purgeList.end();
					itPurge++) {
				std::string eventId = *itPurge;
				repo->logger_->log_info("ProvenanceRepository Repo Purge %s",
						eventId.c_str());
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

