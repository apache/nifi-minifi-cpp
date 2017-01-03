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

#include "Processor.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "Configure.h"

Processor::Processor(std::string name, uuid_t uuid)
: _name(name)
{
	if (!uuid)
		// Generate the global UUID for the flow record
		uuid_generate(_uuid);
	else
		uuid_copy(_uuid, uuid);

	char uuidStr[37];
	uuid_unparse(_uuid, uuidStr);
	_uuidStr = uuidStr;

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
	for (std::set<Property>::iterator it = properties.begin(); it != properties.end(); ++it)
	{
		Property item(*it);
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
	for (std::set<Relationship>::iterator it = relationships.begin(); it != relationships.end(); ++it)
	{
		Relationship item(*it);
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
	for (std::set<Relationship>::iterator it = relationships.begin(); it != relationships.end(); ++it)
	{
		Relationship item(*it);
		_autoTerminatedRelationships[item.getName()] = item;
		_logger->log_info("Processor %s auto terminated relationship name %s", _name.c_str(), item.getName().c_str());
	}

	return true;
}

bool Processor::isAutoTerminated(Relationship relationship)
{
	bool isRun = isRunning();

	if (!isRun)
		_mtx.lock();

	std::map<std::string, Relationship>::iterator it = _autoTerminatedRelationships.find(relationship.getName());
	if (it != _autoTerminatedRelationships.end())
	{
		if (!isRun)
			_mtx.unlock();
		return true;
	}
	else
	{
		if (!isRun)
			_mtx.unlock();
		return false;
	}
}

bool Processor::isSupportedRelationship(Relationship relationship)
{
	bool isRun = isRunning();

	if (!isRun)
		_mtx.lock();

	std::map<std::string, Relationship>::iterator it = _relationships.find(relationship.getName());
	if (it != _relationships.end())
	{
		if (!isRun)
			_mtx.unlock();
		return true;
	}
	else
	{
		if (!isRun)
			_mtx.unlock();
		return false;
	}
}

bool Processor::getProperty(std::string name, std::string &value)
{
	bool isRun = isRunning();

	if (!isRun)
		// Because set property only allowed in non running state, we need to obtain lock avoid rack condition
		_mtx.lock();

	std::map<std::string, Property>::iterator it = _properties.find(name);
	if (it != _properties.end())
	{
		Property item = it->second;
		value = item.getValue();
		if (!isRun)
			_mtx.unlock();
		return true;
	}
	else
	{
		if (!isRun)
			_mtx.unlock();
		return false;
	}
}

bool Processor::getDynamicProperty(std::string name, std::string &value)
{
	if (isRunning())
		// Because set property only allowed in non running state, we need to obtain lock avoid rack condition
		_mtx.lock();

	std::map<std::string, Property>::iterator it = _dynamicProperties.find(name);
	if (it != _dynamicProperties.end())
	{
		Property item = it->second;
		value = item.getValue();
		if (!isRunning())
			_mtx.unlock();
		return true;
	}
	else
	{
		if (!isRunning())
			_mtx.unlock();
		return false;
	}
}

bool Processor::setProperty(std::string name, std::string value)
{

	std::lock_guard<std::mutex> lock(_mtx);
	std::map<std::string, Property>::iterator it = _properties.find(name);

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

bool Processor::setDynamicProperty(std::string name, std::string value)
{
	std::lock_guard<std::mutex> lock(_mtx);
	std::map<std::string, Property>::iterator it = _dynamicProperties.find(name);

	//Does user want Dynamic properties to be accepted?
	std::string acceptDynValue;
	Configure::getConfigure()->get(Configure::getConfigure()->nifi_accept_dynamic_properties, acceptDynValue);

	if (it != _dynamicProperties.end())
	{
		Property item = it->second;
		item.setValue(value);
		_dynamicProperties[item.getName()] = item;
		_logger->log_info("Processor %s dynamic property name %s value %s", _name.c_str(), item.getName().c_str(), value.c_str());
		return true;
	}
	else
	{
		//Create and add the new Property to the list of Dynamic Properties if enabled
		if (acceptDynValue == "true" || acceptDynValue == "True" || acceptDynValue == "TRUE") {
			Property *dyn = new Property(name, DEFAULT_DYNAMIC_PROPERTY_DESC, value);
			_logger->log_info("Processor %s dynamic property '%s' value '%s'", _name.c_str(), dyn->getName().c_str(), value.c_str());
			_dynamicProperties[dyn->getName()] = *dyn;
			return true;
		} else {
			_logger->log_warn("MiNiFi property %s was set to %s so ignoring dynamic property %s with value %s", 
				Configure::getConfigure()->nifi_accept_dynamic_properties, acceptDynValue.c_str(), name.c_str(), value.c_str());
			return false;
		}
	}
}

std::set<Connection *> Processor::getOutGoingConnections(std::string relationship)
{
	std::set<Connection *> empty;

	std::map<std::string, std::set<Connection *>>::iterator it = _outGoingConnections.find(relationship);
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

	if (uuid_compare(_uuid, destUUID) == 0)
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

	if (uuid_compare(_uuid, srcUUID) == 0)
	{
		std::string relationship = connection->getRelationship().getName();
		// Connection is source from the current processor
		std::map<std::string, std::set<Connection *>>::iterator it =
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
		std::map<std::string, std::set<Connection *>>::iterator it =
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

	for (std::set<Connection *>::iterator it = _incomingConnections.begin(); it != _incomingConnections.end(); ++it)
	{
		Connection *connection = *it;
		if (connection->getQueueSize() > 0)
			return true;
	}

	return false;
}

bool Processor::flowFilesOutGoingFull()
{
	std::lock_guard<std::mutex> lock(_mtx);

	std::map<std::string, std::set<Connection *>>::iterator it;

	for (it = _outGoingConnections.begin(); it != _outGoingConnections.end(); ++it)
	{
		// We already has connection for this relationship
		std::set<Connection *> existedConnection = it->second;
		for (std::set<Connection *>::iterator itConnection = existedConnection.begin(); itConnection != existedConnection.end(); ++itConnection)
		{
			Connection *connection = *itConnection;
			if (connection->isFull())
				return true;
		}
	}

	return false;
}

void Processor::onTrigger()
{
	ProcessContext *context = new ProcessContext(this);
	ProcessSession *session = new ProcessSession(context);
	try {
		// Call the child onTrigger function
		this->onTrigger(context, session);
		session->commit();
		delete session;
		delete context;
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		session->rollback();
		delete session;
		delete context;
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception Processor::onTrigger");
		session->rollback();
		delete session;
		delete context;
		throw;
	}
}
