/**
 * @file FlowController.cpp
 * FlowController class implementation
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
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "FlowController.h"
#include "ProcessContext.h"

FlowController::FlowController(std::string name)
: _name(name)
{
	uuid_generate(_uuid);

	// Setup the default values
	_xmlFileName = DEFAULT_FLOW_XML_FILE_NAME;
	_maxEventDrivenThreads = DEFAULT_MAX_EVENT_DRIVEN_THREAD;
	_maxTimerDrivenThreads = DEFAULT_MAX_TIMER_DRIVEN_THREAD;
	_running = false;
	_initialized = false;
	_root = NULL;
	_logger = Logger::getLogger();
	_protocol = new FlowControlProtocol(this);

	// NiFi config properties
	_configure = Configure::getConfigure();
	_configure->get(Configure::nifi_flow_configuration_file, _xmlFileName);
	_logger->log_info("FlowController NiFi XML file %s", _xmlFileName.c_str());
	// Create repos for flow record and provenance

	_logger->log_info("FlowController %s created", _name.c_str());
}

FlowController::~FlowController()
{
	stop(true);
	unload();
}

bool FlowController::isRunning()
{
	return (_running);
}

bool FlowController::isInitialized()
{
	return (_initialized);
}

void FlowController::stop(bool force)
{
	if (_running)
	{
		_logger->log_info("Stop Flow Controller");
		this->_timerScheduler.stop();
		// Wait for sometime for thread stop
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		if (this->_root)
			this->_root->stopProcessing(&this->_timerScheduler);
		_running = false;
	}
}

void FlowController::unload()
{
	if (_running)
	{
		stop(true);
	}
	if (_initialized)
	{
		_logger->log_info("Unload Flow Controller");
		if (_root)
			delete _root;
		_root = NULL;
		_initialized = false;
		_name = "";
	}

	return;
}

void FlowController::reload(std::string xmlFile)
{
	_logger->log_info("Starting to reload Flow Controller with xml %s", xmlFile.c_str());
	stop(true);
	unload();
	std::string oldxmlFile = this->_xmlFileName;
	this->_xmlFileName = xmlFile;
	load();
	start();
	if (!this->_root)
	{
		this->_xmlFileName = oldxmlFile;
		_logger->log_info("Rollback Flow Controller to xml %s", oldxmlFile.c_str());
		stop(true);
		unload();
		load();
		start();
	}
}

Processor *FlowController::createProcessor(std::string name, uuid_t uuid)
{
	Processor *processor = NULL;
	if (name == GenerateFlowFile::ProcessorName)
	{
		processor = new GenerateFlowFile(name, uuid);
	}
	else if (name == LogAttribute::ProcessorName)
	{
		processor = new LogAttribute(name, uuid);
	}
	else if (name == RealTimeDataCollector::ProcessorName)
	{
		processor = new RealTimeDataCollector(name, uuid);
	}
	else if (name == GetFile::ProcessorName)
	{
		processor = new GetFile(name, uuid);
	}
	else if (name == TailFile::ProcessorName)
	{
		processor = new TailFile(name, uuid);
	}
	else if (name == ListenSyslog::ProcessorName)
	{
		processor = new ListenSyslog(name, uuid);
	}
	else
	{
		_logger->log_error("No Processor defined for %s", name.c_str());
		return NULL;
	}

	//! initialize the processor
	processor->initialize();

	return processor;
}

ProcessGroup *FlowController::createRootProcessGroup(std::string name, uuid_t uuid)
{
	return new ProcessGroup(ROOT_PROCESS_GROUP, name, uuid);
}

ProcessGroup *FlowController::createRemoteProcessGroup(std::string name, uuid_t uuid)
{
	return new ProcessGroup(REMOTE_PROCESS_GROUP, name, uuid);
}

Connection *FlowController::createConnection(std::string name, uuid_t uuid)
{
	return new Connection(name, uuid);
}

void FlowController::parseConnection(xmlDoc *doc, xmlNode *node, ProcessGroup *parent)
{
	uuid_t uuid;
	xmlNode *currentNode;
	Connection *connection = NULL;

	if (!parent)
	{
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}

	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0)
			{
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseConnection: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseConnection: name => [%s]", name);
					connection = this->createConnection(name, uuid);
					if (connection == NULL)
					{
						xmlFree(name);
						return;
					}
					xmlFree(name);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "sourceId") == 0)
			{
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseConnection: sourceId => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
					if (connection)
						connection->setSourceProcessorUUID(uuid);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "destinationId") == 0)
			{
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseConnection: destinationId => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
					if (connection)
						connection->setDestinationProcessorUUID(uuid);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "maxWorkQueueSize") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				int64_t maxWorkQueueSize = 0;
				if (temp)
				{
					if (Property::StringToInt(temp, maxWorkQueueSize))
					{
						_logger->log_debug("parseConnection: maxWorkQueueSize => [%d]", maxWorkQueueSize);
						if (connection)
							connection->setMaxQueueSize(maxWorkQueueSize);

					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "maxWorkQueueDataSize") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				int64_t maxWorkQueueDataSize = 0;
				if (temp)
				{
					if (Property::StringToInt(temp, maxWorkQueueDataSize))
					{
						_logger->log_debug("parseConnection: maxWorkQueueDataSize => [%d]", maxWorkQueueDataSize);
						if (connection)
							connection->setMaxQueueDataSize(maxWorkQueueDataSize);

					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "relationship") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					std::string relationshipName = temp;
					if (!relationshipName.empty())
					{
						Relationship relationship(relationshipName, "");
						_logger->log_debug("parseConnection: relationship => [%s]", relationshipName.c_str());
						if (connection)
							connection->setRelationship(relationship);
					}
					else
					{
						Relationship empty;
						_logger->log_debug("parseConnection: relationship => [%s]", empty.getName().c_str());
						if (connection)
							connection->setRelationship(empty);
					}
					xmlFree(temp);
				}
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // for node

	if (connection)
		parent->addConnection(connection);

	return;
}

void FlowController::parseRootProcessGroup(xmlDoc *doc, xmlNode *node)
{
	uuid_t uuid;
	xmlNode *currentNode;
	ProcessGroup *group = NULL;

	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0)
			{
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseRootProcessGroup: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseRootProcessGroup: name => [%s]", name);
					group = this->createRootProcessGroup(name, uuid);
					if (group == NULL)
					{
						xmlFree(name);
						return;
					}
					// Set the root process group
					this->_root = group;
					this->_name = name;
					xmlFree(name);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "processor") == 0)
			{
				this->parseProcessorNode(doc, currentNode, group);
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "connection") == 0)
			{
				this->parseConnection(doc, currentNode, group);
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "remoteProcessGroup") == 0)
			{
				this->parseRemoteProcessGroup(doc, currentNode, group);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
      } // for node
}

void FlowController::parseRemoteProcessGroup(xmlDoc *doc, xmlNode *node, ProcessGroup *parent)
{
	uuid_t uuid;
	xmlNode *currentNode;
	ProcessGroup *group = NULL;
	int64_t yieldPeriod = -1;
	int64_t timeOut = -1;

	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0)
			{
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseRootProcessGroup: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseRemoteProcessGroup: name => [%s]", name);
					group = this->createRemoteProcessGroup(name, uuid);
					if (group == NULL)
					{
						xmlFree(name);
						return;
					}
					group->setParent(parent);
					parent->addProcessGroup(group);
					xmlFree(name);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "yieldPeriod") == 0)
			{
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToTime(temp, yieldPeriod, unit) &&
							Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod) && group)
					{
						_logger->log_debug("parseRemoteProcessGroup: yieldPeriod => [%d] ms", yieldPeriod);
						group->setYieldPeriodMsec(yieldPeriod);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "timeout") == 0)
			{
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToTime(temp, timeOut, unit) &&
							Property::ConvertTimeUnitToMS(timeOut, unit, timeOut) && group)
					{
						_logger->log_debug("parseRemoteProcessGroup: timeOut => [%d] ms", timeOut);
						group->setTimeOut(timeOut);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "transmitting") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				bool transmitting;
				if (temp)
				{
					if (Property::StringToBool(temp, transmitting) && group)
					{
						_logger->log_debug("parseRemoteProcessGroup: transmitting => [%d]", transmitting);
						group->setTransmitting(transmitting);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "inputPort") == 0 && group)
			{
				this->parsePort(doc, currentNode, group, SEND);
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "outputPort") == 0 && group)
			{
				this->parsePort(doc, currentNode, group, RECEIVE);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
      } // for node
}

void FlowController::parseProcessorProperty(xmlDoc *doc, xmlNode *node, Processor *processor)
{
	xmlNode *currentNode;
	std::string propertyValue;
	std::string propertyName;

	if (!processor)
	{
		_logger->log_error("parseProcessorProperty: no parent processor existed");
		return;
	}

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					propertyName = name;
					xmlFree(name);
				}
			}
			if (xmlStrcmp(currentNode->name, BAD_CAST "value") == 0)
			{
				char *value = (char *) xmlNodeGetContent(currentNode);
				if (value)
				{
					_logger->log_debug("parseProcessorNode: value => [%s]", value);
					propertyValue = value;
					xmlFree(value);
				}
			}
			if (!propertyName.empty() && !propertyValue.empty())
			{
				processor->setProperty(propertyName, propertyValue);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
      } // for node
}

void FlowController::parsePort(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent, TransferDirection direction)
{
	char *id = NULL;
	char *name = NULL;
	uuid_t uuid;
	xmlNode *currentNode;
	Processor *processor = NULL;
	RemoteProcessorGroupPort *port = NULL;

	if (!parent)
	{
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}
	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = processorNode->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0)
			{
				id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseProcessorNode: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					port = new RemoteProcessorGroupPort(name, uuid);
					processor = (Processor *) port;
					if (processor == NULL)
					{
						xmlFree(name);
						return;
					}
					port->setDirection(direction);
					port->setTimeOut(parent->getTimeOut());
					port->setTransmitting(parent->getTransmitting());
					processor->setYieldPeriodMsec(parent->getYieldPeriodMsec());
					processor->initialize();
					// add processor to parent
					parent->addProcessor(processor);
					xmlFree(name);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "scheduledState") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					std::string state = temp;
					if (state == "DISABLED")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(DISABLED);
					}
					if (state == "STOPPED")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(STOPPED);
					}
					if (state == "RUNNING")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(RUNNING);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "maxConcurrentTasks") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					int64_t maxConcurrentTasks;
					if (Property::StringToInt(temp, maxConcurrentTasks))
					{
						_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
						processor->setMaxConcurrentTasks(maxConcurrentTasks);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "property") == 0)
			{
				this->parseProcessorProperty(doc, currentNode, processor);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
      } // while node
}

void FlowController::parseProcessorNode(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent)
{
	char *id = NULL;
	char *name = NULL;
	int64_t schedulingPeriod = -1;
	int64_t penalizationPeriod = -1;
	int64_t yieldPeriod = -1;
	bool lossTolerant = false;
	int64_t runDurationNanos = -1;
	uuid_t uuid;
	xmlNode *currentNode;
	Processor *processor = NULL;

	if (!parent)
	{
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}
	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = processorNode->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
	{
		if (currentNode->type == XML_ELEMENT_NODE)
		{
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0)
			{
				id = (char *) xmlNodeGetContent(currentNode);
				if (id)
				{
					_logger->log_debug("parseProcessorNode: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0)
			{
				name = (char *) xmlNodeGetContent(currentNode);
				if (name)
				{
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					processor = this->createProcessor(name, uuid);
					if (processor == NULL)
					{
						xmlFree(name);
						return;
					}
					// add processor to parent
					parent->addProcessor(processor);
					xmlFree(name);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "schedulingPeriod") == 0)
			{
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToTime(temp, schedulingPeriod, unit) &&
							Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod))
					{
						_logger->log_debug("parseProcessorNode: schedulingPeriod => [%d] ns", schedulingPeriod);
						processor->setSchedulingPeriodNano(schedulingPeriod);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "penalizationPeriod") == 0)
			{
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToTime(temp, penalizationPeriod, unit) &&
							Property::ConvertTimeUnitToMS(penalizationPeriod, unit, penalizationPeriod))
					{
						_logger->log_debug("parseProcessorNode: penalizationPeriod => [%d] ms", penalizationPeriod);
						processor->setPenalizationPeriodMsec(penalizationPeriod);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "yieldPeriod") == 0)
			{
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToTime(temp, yieldPeriod, unit) &&
							Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod))
					{
						_logger->log_debug("parseProcessorNode: yieldPeriod => [%d] ms", yieldPeriod);
						processor->setYieldPeriodMsec(yieldPeriod);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "lossTolerant") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToBool(temp, lossTolerant))
					{
						_logger->log_debug("parseProcessorNode: lossTolerant => [%d]", lossTolerant);
						processor->setlossTolerant(lossTolerant);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "scheduledState") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					std::string state = temp;
					if (state == "DISABLED")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(DISABLED);
					}
					if (state == "STOPPED")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(STOPPED);
					}
					if (state == "RUNNING")
					{
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(RUNNING);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "schedulingStrategy") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					std::string strategy = temp;
					if (strategy == "TIMER_DRIVEN")
					{
						_logger->log_debug("parseProcessorNode: scheduledStrategy  => [%s]", strategy.c_str());
						processor->setSchedulingStrategy(TIMER_DRIVEN);
					}
					if (strategy == "EVENT_DRIVEN")
					{
						_logger->log_debug("parseProcessorNode: scheduledStrategy  => [%s]", strategy.c_str());
						processor->setSchedulingStrategy(EVENT_DRIVEN);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "maxConcurrentTasks") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					int64_t maxConcurrentTasks;
					if (Property::StringToInt(temp, maxConcurrentTasks))
					{
						_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
						processor->setMaxConcurrentTasks(maxConcurrentTasks);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "runDurationNanos") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					if (Property::StringToInt(temp, runDurationNanos))
					{
						_logger->log_debug("parseProcessorNode: runDurationNanos => [%d]", runDurationNanos);
						processor->setRunDurationNano(runDurationNanos);
					}
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "autoTerminatedRelationship") == 0)
			{
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp)
				{
					std::string relationshipName = temp;
					Relationship relationship(relationshipName, "");
					std::set<Relationship> relationships;

					relationships.insert(relationship);
					processor->setAutoTerminatedRelationships(relationships);
					_logger->log_debug("parseProcessorNode: autoTerminatedRelationship  => [%s]", relationshipName.c_str());
					xmlFree(temp);
				}
			}
			else if (xmlStrcmp(currentNode->name, BAD_CAST "property") == 0)
			{
				this->parseProcessorProperty(doc, currentNode, processor);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
      } // while node
}

void FlowController::load()
{
	if (_running)
	{
		stop(true);
	}
	if (!_initialized)
	{
		_logger->log_info("Load Flow Controller from file %s", _xmlFileName.c_str());

		xmlDoc *doc = xmlReadFile(_xmlFileName.c_str(), NULL, XML_PARSE_NONET);
		if (doc == NULL)
		{
			_logger->log_error("xmlReadFile returned NULL when reading [%s]", _xmlFileName.c_str());
			_initialized = true;
			return;
		}

		xmlNode *root = xmlDocGetRootElement(doc);

		if (root == NULL)
		{
			_logger->log_error("Can not get root from XML doc %s", _xmlFileName.c_str());
			xmlFreeDoc(doc);
			xmlCleanupParser();
		}

		if (xmlStrcmp(root->name, BAD_CAST "flowController") != 0)
		{
			_logger->log_error("Root name is not flowController for XML doc %s", _xmlFileName.c_str());
			xmlFreeDoc(doc);
			xmlCleanupParser();
			return;
		}

		xmlNode *currentNode;

		for (currentNode = root->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next)
		{
			if (currentNode->type == XML_ELEMENT_NODE)
			{
				if (xmlStrcmp(currentNode->name, BAD_CAST "rootGroup") == 0)
				{
					this->parseRootProcessGroup(doc, currentNode);
				}
				else if (xmlStrcmp(currentNode->name, BAD_CAST "maxTimerDrivenThreadCount") == 0)
				{
					char *temp = (char *) xmlNodeGetContent(currentNode);
					int64_t maxTimerDrivenThreadCount;
					if (temp)
					{
						if (Property::StringToInt(temp, maxTimerDrivenThreadCount))
						{
							_logger->log_debug("maxTimerDrivenThreadCount => [%d]", maxTimerDrivenThreadCount);
							this->_maxTimerDrivenThreads = maxTimerDrivenThreadCount;
						}
						xmlFree(temp);
					}
				}
				else if (xmlStrcmp(currentNode->name, BAD_CAST "maxEventDrivenThreadCount") == 0)
				{
					char *temp = (char *) xmlNodeGetContent(currentNode);
					int64_t maxEventDrivenThreadCount;
					if (temp)
					{
						if (Property::StringToInt(temp, maxEventDrivenThreadCount))
						{
							_logger->log_debug("maxEventDrivenThreadCount => [%d]", maxEventDrivenThreadCount);
							this->_maxEventDrivenThreads = maxEventDrivenThreadCount;
						}
						xmlFree(temp);
					}
				}
			} // type == XML_ELEMENT_NODE
		} // for

		xmlFreeDoc(doc);
		xmlCleanupParser();
		_initialized = true;
	}
}

bool FlowController::start()
{
	if (!_initialized)
	{
		_logger->log_error("Can not start Flow Controller because it has not been initialized");
		return false;
	}
	else
	{
		if (!_running)
		{
			_logger->log_info("Start Flow Controller");
			this->_timerScheduler.start();
			if (this->_root)
				this->_root->startProcessing(&this->_timerScheduler);
			_running = true;
			this->_protocol->start();
		}
		return true;
	}
}
