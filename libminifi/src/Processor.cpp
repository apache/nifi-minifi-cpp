/**
 * @file Processor.cpp
 * Processor class implementation
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
#include <memory>
#include <functional>

#include "Processor.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessSessionFactory.h"

Processor::Processor(std::string name, uuid_t uuid)
: _name(name)
, _processContext(new ProcessContext(this))
, _processSessionFactory(new ProcessSessionFactory(_processContext.get()))
{
	if (!uuid)
		// Generate the global UUID for the flow record
		uuid_generate(_uuid);
	else
		uuid_copy(_uuid, uuid);

	char uuidStr[37];
	uuid_unparse_lower(_uuid, uuidStr);
	_uuidStr = uuidStr;
	_hasWork.store(false);
	// Setup the default values
	_state = DISABLED;
	_strategy = TIMER_DRIVEN;
	_lossTolerant = false;
	_triggerWhenEmpty = false;
	_schedulingPeriodNano = MINIMUM_SCHEDULING_NANOS;
	_runDurantionNano = 0;
	_yieldPeriodMsec = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
	_penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
	_maxConcurrentTasks = 1;
	_activeTasks = 0;
	_yieldExpiration = 0;
	_incomingConnectionsIter = this->_incomingConnections.begin();
	_logger = Logger::getLogger();
	_logger->log_info("Processor %s created UUID %s", _name.c_str(), _uuidStr.c_str());
}

Processor::~Processor()
{

}

bool Processor::isRunning()
{
	return (_state == RUNNING && _activeTasks > 0);
}

void Processor::setScheduledState(ScheduledState state)
{
	// Do nothing if there is no state change
	if (state == _state)
	{
		return;
	}

	_state = state;

	// Invoke the onSchedule hook
	if (state == RUNNING)
	{
		onSchedule(_processContext.get(), _processSessionFactory.get());
	}
}

bool Processor::setSupportedProperties(std::set<Property> properties)
{
	if (isRunning())
	{
		_logger->log_info("Can not set processor property while the process %s is running",
				_name.c_str());
		return false;
	}

	std::lock_guard<std::mutex> lock(_mtx);

	_properties.clear();
	for (auto item : properties)
	{
		_properties[item.getName()] = item;
		_logger->log_info("Processor %s supported property name %s", _name.c_str(), item.getName().c_str());
	}

	return true;
}

bool Processor::setSupportedRelationships(std::set<Relationship> relationships)
{
	if (isRunning())
	{
		_logger->log_info("Can not set processor supported relationship while the process %s is running",
				_name.c_str());
		return false;
	}

	std::lock_guard<std::mutex> lock(_mtx);

	_relationships.clear();
	for(auto item : relationships)
	{
		_relationships[item.getName()] = item;
		_logger->log_info("Processor %s supported relationship name %s", _name.c_str(), item.getName().c_str());
	}

	return true;
}

bool Processor::setAutoTerminatedRelationships(std::set<Relationship> relationships)
{
	if (isRunning())
	{
		_logger->log_info("Can not set processor auto terminated relationship while the process %s is running",
				_name.c_str());
		return false;
	}

	std::lock_guard<std::mutex> lock(_mtx);

	_autoTerminatedRelationships.clear();
	for(auto item : relationships)
	{
		_autoTerminatedRelationships[item.getName()] = item;
		_logger->log_info("Processor %s auto terminated relationship name %s", _name.c_str(), item.getName().c_str());
	}

	return true;
}

bool Processor::isAutoTerminated(Relationship relationship)
{
	bool isRun = isRunning();
		
	auto conditionalLock = !isRun ? 
			  std::unique_lock<std::mutex>() 
			: std::unique_lock<std::mutex>(_mtx);

	const auto &it = _autoTerminatedRelationships.find(relationship.getName());
	if (it != _autoTerminatedRelationships.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool Processor::isSupportedRelationship(Relationship relationship)
{
	bool isRun = isRunning();

	auto conditionalLock = !isRun ? 
			  std::unique_lock<std::mutex>() 
			: std::unique_lock<std::mutex>(_mtx);

	const auto &it = _relationships.find(relationship.getName());
	if (it != _relationships.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool Processor::getProperty(std::string name, std::string &value)
{
	bool isRun = isRunning();

	
	 auto conditionalLock = !isRun ? 
                           std::unique_lock<std::mutex>() 
                         : std::unique_lock<std::mutex>(_mtx);
			 
	const auto &it = _properties.find(name);
	if (it != _properties.end())
	{
		Property item = it->second;
		value = item.getValue();
		return true;
	}
	else
	{
		return false;
	}
}

bool Processor::setProperty(std::string name, std::string value)
{

	std::lock_guard<std::mutex> lock(_mtx);
	auto &&it = _properties.find(name);

	if (it != _properties.end())
	{
		Property item = it->second;
		item.setValue(value);
		_properties[item.getName()] = item;
		_logger->log_info("Processor %s property name %s value %s", _name.c_str(), item.getName().c_str(), value.c_str());
		return true;
	}
	else
	{
		return false;
	}
}

bool Processor::setProperty(Property prop, std::string value) {

	std::lock_guard<std::mutex> lock(_mtx);
	auto it = _properties.find(
			prop.getName());

	if (it != _properties.end()) {
		Property item = it->second;
		item.setValue(value);
		_properties[item.getName()] = item;
		_logger->log_info("Processor %s property name %s value %s",
				_name.c_str(), item.getName().c_str(), value.c_str());
		return true;
	} else {
		Property newProp(prop);
		newProp.setValue(value);
		_properties.insert(
				std::pair<std::string, Property>(prop.getName(), newProp));
		return true;

		return false;
	}
}

std::set<Connection *> Processor::getOutGoingConnections(std::string relationship)
{
	std::set<Connection *> empty;

	auto  &&it = _outGoingConnections.find(relationship);
	if (it != _outGoingConnections.end())
	{
		return _outGoingConnections[relationship];
	}
	else
	{
		return empty;
	}
}

bool Processor::addConnection(Connection *connection)
{
	bool ret = false;


	if (isRunning())
	{
		_logger->log_info("Can not add connection while the process %s is running",
				_name.c_str());
		return false;
	}


	std::lock_guard<std::mutex> lock(_mtx);

	uuid_t srcUUID;
	uuid_t destUUID;

	connection->getSourceProcessorUUID(srcUUID);
	connection->getDestinationProcessorUUID(destUUID);
	char uuid_str[37];


	uuid_unparse_lower(_uuid, uuid_str);
	std::string my_uuid = uuid_str;
	uuid_unparse_lower(destUUID, uuid_str);
	std::string destination_uuid = uuid_str;
	if (my_uuid == destination_uuid)
	{
		// Connection is destination to the current processor
		if (_incomingConnections.find(connection) == _incomingConnections.end())
		{
			_incomingConnections.insert(connection);
			connection->setDestinationProcessor(this);
			_logger->log_info("Add connection %s into Processor %s incoming connection",
					connection->getName().c_str(), _name.c_str());
			_incomingConnectionsIter = this->_incomingConnections.begin();
			ret = true;
		}
	}
	uuid_unparse_lower(srcUUID, uuid_str);
	std::string source_uuid = uuid_str;
	if (my_uuid == source_uuid)
	{
		std::string relationship = connection->getRelationship().getName();
		// Connection is source from the current processor
		auto &&it =
				_outGoingConnections.find(relationship);
		if (it != _outGoingConnections.end())
		{
			// We already has connection for this relationship
			std::set<Connection *> existedConnection = it->second;
			if (existedConnection.find(connection) == existedConnection.end())
			{
				// We do not have the same connection for this relationship yet
				existedConnection.insert(connection);
				connection->setSourceProcessor(this);
				_outGoingConnections[relationship] = existedConnection;
				_logger->log_info("Add connection %s into Processor %s outgoing connection for relationship %s",
												connection->getName().c_str(), _name.c_str(), relationship.c_str());
				ret = true;
			}
		}
		else
		{

			// We do not have any outgoing connection for this relationship yet
			std::set<Connection *> newConnection;
			newConnection.insert(connection);
			connection->setSourceProcessor(this);
			_outGoingConnections[relationship] = newConnection;
			_logger->log_info("Add connection %s into Processor %s outgoing connection for relationship %s",
								connection->getName().c_str(), _name.c_str(), relationship.c_str());
			ret = true;
		}
	}
	

	return ret;
}

void Processor::removeConnection(Connection *connection)
{
	if (isRunning())
	{
		_logger->log_info("Can not remove connection while the process %s is running",
				_name.c_str());
		return;
	}

	std::lock_guard<std::mutex> lock(_mtx);

	uuid_t srcUUID;
	uuid_t destUUID;

	connection->getSourceProcessorUUID(srcUUID);
	connection->getDestinationProcessorUUID(destUUID);

	if (uuid_compare(_uuid, destUUID) == 0)
	{
		// Connection is destination to the current processor
		if (_incomingConnections.find(connection) != _incomingConnections.end())
		{
			_incomingConnections.erase(connection);
			connection->setDestinationProcessor(NULL);
			_logger->log_info("Remove connection %s into Processor %s incoming connection",
					connection->getName().c_str(), _name.c_str());
			_incomingConnectionsIter = this->_incomingConnections.begin();
		}
	}

	if (uuid_compare(_uuid, srcUUID) == 0)
	{
		std::string relationship = connection->getRelationship().getName();
		// Connection is source from the current processor
		auto &&it =
				_outGoingConnections.find(relationship);
		if (it == _outGoingConnections.end())
		{
			return;
		}
		else
		{
			if (_outGoingConnections[relationship].find(connection) != _outGoingConnections[relationship].end())
			{
				_outGoingConnections[relationship].erase(connection);
				connection->setSourceProcessor(NULL);
				_logger->log_info("Remove connection %s into Processor %s outgoing connection for relationship %s",
								connection->getName().c_str(), _name.c_str(), relationship.c_str());
			}
		}
	}
}

Connection *Processor::getNextIncomingConnection()
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_incomingConnections.size() == 0)
		return NULL;

	if (_incomingConnectionsIter == _incomingConnections.end())
		_incomingConnectionsIter = _incomingConnections.begin();

	Connection *ret = *_incomingConnectionsIter;
	_incomingConnectionsIter++;

	if (_incomingConnectionsIter == _incomingConnections.end())
		_incomingConnectionsIter = _incomingConnections.begin();

	return ret;
}

bool Processor::flowFilesQueued()
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_incomingConnections.size() == 0)
		return false;

	for(auto &&connection : _incomingConnections)
	{
		if (connection->getQueueSize() > 0)
			return true;
	}

	return false;
}

bool Processor::flowFilesOutGoingFull()
{
	std::lock_guard<std::mutex> lock(_mtx);

	 for(auto &&connection : _outGoingConnections)
	{
		// We already has connection for this relationship
		std::set<Connection *> existedConnection = connection.second;
		for(const auto connection : existedConnection)
		{
			if (connection->isFull())
				return true;
		}
	}

	return false;
}

void Processor::onTrigger()
{
	auto session = _processSessionFactory->createSession();

	try {
		// Call the session function
		onTrigger(_processContext.get(), session.get());
		session->commit();
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		session->rollback();
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception Processor::onTrigger");
		session->rollback();
		throw;
	}
}

void Processor::waitForWork(uint64_t timeoutMs)
{
	_hasWork.store( isWorkAvailable() );

	if (!_hasWork.load())
	{
	    std::unique_lock<std::mutex> lock(_workAvailableMtx);
	    _hasWorkCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return _hasWork.load(); });
	}

}

void Processor::notifyWork()
{
	// Do nothing if we are not event-driven
	if (_strategy != EVENT_DRIVEN)
	{
		return;
	}

	{
		_hasWork.store( isWorkAvailable() );


		if (_hasWork.load())
		{
		      _hasWorkCondition.notify_one();
		}
	}
}

bool Processor::isWorkAvailable()
{
	// We have work if any incoming connection has work
	bool hasWork = false;

	try
	{
		for (const auto &conn : getIncomingConnections())
		{
			if (conn->getQueueSize() > 0)
			{
				hasWork = true;
				break;
			}
		}
	}
	catch (...)
	{
		_logger->log_error("Caught an exception while checking if work is available; unless it was positively determined that work is available, assuming NO work is available!");
	}

	return hasWork;
}
