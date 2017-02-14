/**
 * @file ProcessGroup.cpp
 * ProcessGroup class implementation
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
#include <set>
#include <sys/time.h>
#include <time.h>
#include <chrono>
#include <thread>

#include "ProcessGroup.h"
#include "Processor.h"

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, uuid_t uuid,
		ProcessGroup *parent) :
		name_(name), type_(type), parent_process_group_(parent) {
	if (!uuid)
		// Generate the global UUID for the flow record
		uuid_generate(uuid_);
	else
		uuid_copy(uuid_, uuid);

	yield_period_msec_ = 0;
	transmitting_ = false;

	logger_ = Logger::getLogger();
	logger_->log_info("ProcessGroup %s created", name_.c_str());
}

ProcessGroup::~ProcessGroup() {
	for (std::set<Connection *>::iterator it = connections_.begin();
			it != connections_.end(); ++it) {
		Connection *connection = *it;
		connection->drain();
		delete connection;
	}

	for (std::set<ProcessGroup *>::iterator it = child_process_groups_.begin();
			it != child_process_groups_.end(); ++it) {
		ProcessGroup *processGroup(*it);
		delete processGroup;
	}

	for (std::set<Processor *>::iterator it = processors_.begin();
			it != processors_.end(); ++it) {
		Processor *processor(*it);
		delete processor;
	}
}

bool ProcessGroup::isRootProcessGroup() {
	std::lock_guard<std::mutex> lock(mtx_);
	return (type_ == ROOT_PROCESS_GROUP);
}

void ProcessGroup::addProcessor(Processor *processor) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (processors_.find(processor) == processors_.end()) {
		// We do not have the same processor in this process group yet
		processors_.insert(processor);
		logger_->log_info("Add processor %s into process group %s",
				processor->getName().c_str(), name_.c_str());
	}
}

void ProcessGroup::removeProcessor(Processor *processor) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (processors_.find(processor) != processors_.end()) {
		// We do have the same processor in this process group yet
		processors_.erase(processor);
		logger_->log_info("Remove processor %s from process group %s",
				processor->getName().c_str(), name_.c_str());
	}
}

void ProcessGroup::addProcessGroup(ProcessGroup *child) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (child_process_groups_.find(child) == child_process_groups_.end()) {
		// We do not have the same child process group in this process group yet
		child_process_groups_.insert(child);
		logger_->log_info("Add child process group %s into process group %s",
				child->getName().c_str(), name_.c_str());
	}
}

void ProcessGroup::removeProcessGroup(ProcessGroup *child) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (child_process_groups_.find(child) != child_process_groups_.end()) {
		// We do have the same child process group in this process group yet
		child_process_groups_.erase(child);
		logger_->log_info("Remove child process group %s from process group %s",
				child->getName().c_str(), name_.c_str());
	}
}

void ProcessGroup::startProcessing(TimerDrivenSchedulingAgent *timeScheduler,
		EventDrivenSchedulingAgent *eventScheduler) {
	std::lock_guard<std::mutex> lock(mtx_);

	try {
		// Start all the processor node, input and output ports
		for (auto processor : processors_) {
			logger_->log_debug("Starting %s", processor->getName().c_str());

			if (!processor->isRunning()
					&& processor->getScheduledState() != DISABLED) {
				if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
					timeScheduler->schedule(processor);
				else if (processor->getSchedulingStrategy() == EVENT_DRIVEN)
					eventScheduler->schedule(processor);
			}
		}
		// Start processing the group
		for (auto processGroup : child_process_groups_) {
			processGroup->startProcessing(timeScheduler, eventScheduler);
		}
	} catch (std::exception &exception) {
		logger_->log_debug("Caught Exception %s", exception.what());
		throw;
	} catch (...) {
		logger_->log_debug(
				"Caught Exception during process group start processing");
		throw;
	}
}

void ProcessGroup::stopProcessing(TimerDrivenSchedulingAgent *timeScheduler,
		EventDrivenSchedulingAgent *eventScheduler) {
	std::lock_guard<std::mutex> lock(mtx_);

	try {
		// Stop all the processor node, input and output ports
		for (std::set<Processor *>::iterator it = processors_.begin();
				it != processors_.end(); ++it) {
			Processor *processor(*it);
			if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
				timeScheduler->unschedule(processor);
			else if (processor->getSchedulingStrategy() == EVENT_DRIVEN)
				eventScheduler->unschedule(processor);
		}

		for (std::set<ProcessGroup *>::iterator it =
				child_process_groups_.begin(); it != child_process_groups_.end();
				++it) {
			ProcessGroup *processGroup(*it);
			processGroup->stopProcessing(timeScheduler, eventScheduler);
		}
	} catch (std::exception &exception) {
		logger_->log_debug("Caught Exception %s", exception.what());
		throw;
	} catch (...) {
		logger_->log_debug(
				"Caught Exception during process group stop processing");
		throw;
	}
}

Processor *ProcessGroup::findProcessor(uuid_t uuid) {

	Processor *ret = NULL;
	// std::lock_guard<std::mutex> lock(_mtx);

	for(auto processor : processors_){
		logger_->log_info("find processor %s", processor->getName().c_str());
		uuid_t processorUUID;

		if (processor->getUUID(processorUUID)) {

			char uuid_str[37]; // ex. "1b4e28ba-2fa1-11d2-883f-0016d3cca427" + "\0"
			uuid_unparse_lower(processorUUID, uuid_str);
			std::string processorUUIDstr = uuid_str;
			uuid_unparse_lower(uuid, uuid_str);
			std::string uuidStr = uuid_str;
			if (processorUUIDstr == uuidStr) {
				return processor;
			}
		}

	}
	for(auto processGroup : child_process_groups_){

		logger_->log_info("find processor child %s",
				processGroup->getName().c_str());
		Processor *processor = processGroup->findProcessor(uuid);
		if (processor)
			return processor;
	}

	return ret;
}

Processor *ProcessGroup::findProcessor(std::string processorName) {
	Processor *ret = NULL;

	for(auto processor : processors_){
		logger_->log_debug("Current processor is %s",
				processor->getName().c_str());
		if (processor->getName() == processorName)
			return processor;
	}

	for(auto processGroup : child_process_groups_){
		Processor *processor = processGroup->findProcessor(processorName);
		if (processor)
			return processor;
	}

	return ret;
}

void ProcessGroup::updatePropertyValue(std::string processorName,
		std::string propertyName, std::string propertyValue) {
	std::lock_guard<std::mutex> lock(mtx_);

	for(auto processor : processors_){
		if (processor->getName() == processorName) {
			processor->setProperty(propertyName, propertyValue);
		}
	}

	for(auto processGroup : child_process_groups_){
		processGroup->updatePropertyValue(processorName, propertyName,
				propertyValue);
	}

	return;
}

void ProcessGroup::addConnection(Connection *connection) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (connections_.find(connection) == connections_.end()) {
		// We do not have the same connection in this process group yet
		connections_.insert(connection);
		logger_->log_info("Add connection %s into process group %s",
				connection->getName().c_str(), name_.c_str());
		uuid_t sourceUUID;
		Processor *source = NULL;
		connection->getSourceProcessorUUID(sourceUUID);
		source = this->findProcessor(sourceUUID);
		if (source)
			source->addConnection(connection);
		Processor *destination = NULL;
		uuid_t destinationUUID;
		connection->getDestinationProcessorUUID(destinationUUID);
		destination = this->findProcessor(destinationUUID);
		if (destination && destination != source)
			destination->addConnection(connection);
	}
}

void ProcessGroup::removeConnection(Connection *connection) {
	std::lock_guard<std::mutex> lock(mtx_);

	if (connections_.find(connection) != connections_.end()) {
		// We do not have the same connection in this process group yet
		connections_.erase(connection);
		logger_->log_info("Remove connection %s into process group %s",
				connection->getName().c_str(), name_.c_str());
		uuid_t sourceUUID;
		Processor *source = NULL;
		connection->getSourceProcessorUUID(sourceUUID);
		source = this->findProcessor(sourceUUID);
		if (source)
			source->removeConnection(connection);
		Processor *destination = NULL;
		uuid_t destinationUUID;
		connection->getDestinationProcessorUUID(destinationUUID);
		destination = this->findProcessor(destinationUUID);
		if (destination && destination != source)
			destination->removeConnection(connection);
	}
}
