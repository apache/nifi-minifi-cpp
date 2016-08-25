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

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, uuid_t uuid, ProcessGroup *parent)
: _name(name),
  _type(type),
  _parentProcessGroup(parent)
{
	if (!uuid)
		// Generate the global UUID for the flow record
		uuid_generate(_uuid);
	else
		uuid_copy(_uuid, uuid);

	_yieldPeriodMsec = 0;
	_transmitting = false;

	_logger = Logger::getLogger();
	_logger->log_info("ProcessGroup %s created", _name.c_str());
}

ProcessGroup::~ProcessGroup()
{
	for (std::set<Connection *>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		Connection *connection = *it;
		connection->drain();
		delete connection;
	}

	for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
	{
		ProcessGroup *processGroup(*it);
		delete processGroup;
	}

	for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
	{
		Processor *processor(*it);
		delete processor;
	}
}

bool ProcessGroup::isRootProcessGroup()
{
	std::lock_guard<std::mutex> lock(_mtx);
	return (_type == ROOT_PROCESS_GROUP);
}

void ProcessGroup::addProcessor(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_processors.find(processor) == _processors.end())
	{
		// We do not have the same processor in this process group yet
		_processors.insert(processor);
		_logger->log_info("Add processor %s into process group %s",
				processor->getName().c_str(), _name.c_str());
	}
}

void ProcessGroup::removeProcessor(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_processors.find(processor) != _processors.end())
	{
		// We do have the same processor in this process group yet
		_processors.erase(processor);
		_logger->log_info("Remove processor %s from process group %s",
				processor->getName().c_str(), _name.c_str());
	}
}

void ProcessGroup::addProcessGroup(ProcessGroup *child)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_childProcessGroups.find(child) == _childProcessGroups.end())
	{
		// We do not have the same child process group in this process group yet
		_childProcessGroups.insert(child);
		_logger->log_info("Add child process group %s into process group %s",
				child->getName().c_str(), _name.c_str());
	}
}

void ProcessGroup::removeProcessGroup(ProcessGroup *child)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_childProcessGroups.find(child) != _childProcessGroups.end())
	{
		// We do have the same child process group in this process group yet
		_childProcessGroups.erase(child);
		_logger->log_info("Remove child process group %s from process group %s",
				child->getName().c_str(), _name.c_str());
	}
}

void ProcessGroup::startProcessing(TimerDrivenSchedulingAgent *timeScheduler)
{
	std::lock_guard<std::mutex> lock(_mtx);

	try
	{
		// Start all the processor node, input and output ports
		for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
		{
			Processor *processor(*it);
			if (!processor->isRunning() && processor->getScheduledState() != DISABLED)
			{
				if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
					timeScheduler->schedule(processor);
			}
		}

		for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
		{
			ProcessGroup *processGroup(*it);
			processGroup->startProcessing(timeScheduler);
		}
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception during process group start processing");
		throw;
	}
}

void ProcessGroup::stopProcessing(TimerDrivenSchedulingAgent *timeScheduler)
{
	std::lock_guard<std::mutex> lock(_mtx);

	try
	{
		// Stop all the processor node, input and output ports
		for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
		{
			Processor *processor(*it);
			if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
					timeScheduler->unschedule(processor);
		}

		for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
		{
			ProcessGroup *processGroup(*it);
			processGroup->stopProcessing(timeScheduler);
		}
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception during process group stop processing");
		throw;
	}
}

Processor *ProcessGroup::findProcessor(uuid_t uuid)
{
	Processor *ret = NULL;
	// std::lock_guard<std::mutex> lock(_mtx);

	for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
	{
		Processor *processor(*it);
		uuid_t processorUUID;
		if (processor->getUUID(processorUUID) && uuid_compare(processorUUID, uuid) == 0)
			return processor;
	}

	for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
	{
		ProcessGroup *processGroup(*it);
		Processor *processor = processGroup->findProcessor(uuid);
		if (processor)
			return processor;
	}

	return ret;
}

Processor *ProcessGroup::findProcessor(std::string processorName)
{
	Processor *ret = NULL;

	for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
	{
		Processor *processor(*it);
		_logger->log_debug("Current processor is %s", processor->getName().c_str());
		if (processor->getName() == processorName)
			return processor;
	}

	for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
	{
		ProcessGroup *processGroup(*it);
		Processor *processor = processGroup->findProcessor(processorName);
		if (processor)
			return processor;
	}

	return ret;
}

void ProcessGroup::updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue)
{
	std::lock_guard<std::mutex> lock(_mtx);

	for (std::set<Processor *>::iterator it = _processors.begin(); it != _processors.end(); ++it)
	{
		Processor *processor(*it);
		if (processor->getName() == processorName)
		{
			processor->setProperty(propertyName, propertyValue);
		}
	}

	for (std::set<ProcessGroup *>::iterator it = _childProcessGroups.begin(); it != _childProcessGroups.end(); ++it)
	{
		ProcessGroup *processGroup(*it);
		processGroup->updatePropertyValue(processorName, propertyName, propertyValue);
	}

	return;
}

void ProcessGroup::addConnection(Connection *connection)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_connections.find(connection) == _connections.end())
	{
		// We do not have the same connection in this process group yet
		_connections.insert(connection);
		_logger->log_info("Add connection %s into process group %s",
				connection->getName().c_str(), _name.c_str());
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

void ProcessGroup::removeConnection(Connection *connection)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_connections.find(connection) != _connections.end())
	{
		// We do not have the same connection in this process group yet
		_connections.erase(connection);
		_logger->log_info("Remove connection %s into process group %s",
				connection->getName().c_str(), _name.c_str());
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
