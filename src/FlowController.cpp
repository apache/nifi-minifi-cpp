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
	_configurationFileName = DEFAULT_FLOW_YAML_FILE_NAME;
	_maxEventDrivenThreads = DEFAULT_MAX_EVENT_DRIVEN_THREAD;
	_maxTimerDrivenThreads = DEFAULT_MAX_TIMER_DRIVEN_THREAD;
	_running = false;
	_initialized = false;
	_root = NULL;
	_logger = Logger::getLogger();

	// NiFi config properties
	_configure = Configure::getConfigure();

	std::string rawConfigFileString;
	_configure->get(Configure::nifi_flow_configuration_file, rawConfigFileString);

	if (!rawConfigFileString.empty())
	{
		_configurationFileName = rawConfigFileString;
	}

	char *path = NULL;
	char full_path[PATH_MAX];

	std::string adjustedFilename;
	if (!_configurationFileName.empty())
	{
		// perform a naive determination if this is a relative path
		if (_configurationFileName.c_str()[0] != '/')
		{
			adjustedFilename = adjustedFilename + _configure->getHome() + "/" + _configurationFileName;
		}
		else
		{
			adjustedFilename = _configurationFileName;
		}
	}

	path = realpath(adjustedFilename.c_str(), full_path);
	if (!path)
	{
		_logger->log_error("Could not locate path from provided configuration file name.");
	}

	char *flowPath = NULL;
	char flow_full_path[PATH_MAX];

	std::string pathString(path);
	_configurationFileName = pathString;
	_logger->log_info("FlowController NiFi Configuration file %s", pathString.c_str());

	// Create repos for flow record and provenance

	_logger->log_info("FlowController %s created", _name.c_str());
}

FlowController::~FlowController()
{
	stop(true);
	unload();
	delete _protocol;
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
	std::string oldxmlFile = this->_configurationFileName;
	this->_configurationFileName = xmlFile;
	load(ConfigFormat::XML);
	start();
	if (!this->_root)
	{
		this->_configurationFileName = oldxmlFile;
		_logger->log_info("Rollback Flow Controller to xml %s", oldxmlFile.c_str());
		stop(true);
		unload();
		load(ConfigFormat::XML);
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
				if (id) {
					_logger->log_debug("parseConnection: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseConnection: name => [%s]", name);
					connection = this->createConnection(name, uuid);
					if (connection == NULL) {
						xmlFree(name);
						return;
					}
					xmlFree(name);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "sourceId") == 0) {
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseConnection: sourceId => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
					if (connection)
						connection->setSourceProcessorUUID(uuid);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "destinationId") == 0) {
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseConnection: destinationId => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
					if (connection)
						connection->setDestinationProcessorUUID(uuid);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxWorkQueueSize") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				int64_t maxWorkQueueSize = 0;
				if (temp) {
					if (Property::StringToInt(temp, maxWorkQueueSize)) {
						_logger->log_debug("parseConnection: maxWorkQueueSize => [%d]", maxWorkQueueSize);
						if (connection)
							connection->setMaxQueueSize(maxWorkQueueSize);

					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxWorkQueueDataSize") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				int64_t maxWorkQueueDataSize = 0;
				if (temp) {
					if (Property::StringToInt(temp, maxWorkQueueDataSize)) {
						_logger->log_debug("parseConnection: maxWorkQueueDataSize => [%d]", maxWorkQueueDataSize);
						if (connection)
							connection->setMaxQueueDataSize(maxWorkQueueDataSize);

					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "relationship") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					std::string relationshipName = temp;
					if (!relationshipName.empty()) {
						Relationship relationship(relationshipName, "");
						_logger->log_debug("parseConnection: relationship => [%s]", relationshipName.c_str());
						if (connection)
							connection->setRelationship(relationship);
					} else {
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

void FlowController::parseRootProcessGroup(xmlDoc *doc, xmlNode *node) {
	uuid_t uuid;
	xmlNode *currentNode;
	ProcessGroup *group = NULL;

	// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
		if (currentNode->type == XML_ELEMENT_NODE) {
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0) {
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseRootProcessGroup: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseRootProcessGroup: name => [%s]", name);
					group = this->createRootProcessGroup(name, uuid);
					if (group == NULL) {
						xmlFree(name);
						return;
					}
					// Set the root process group
					this->_root = group;
					this->_name = name;
					xmlFree(name);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "processor") == 0) {
				this->parseProcessorNode(doc, currentNode, group);
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "connection") == 0) {
				this->parseConnection(doc, currentNode, group);
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "remoteProcessGroup") == 0) {
				this->parseRemoteProcessGroup(doc, currentNode, group);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // for node
}

void FlowController::parseRootProcessGroupYaml(YAML::Node rootFlowNode) {
	uuid_t uuid;
	ProcessGroup *group = NULL;

	// generate the random UIID
	uuid_generate(uuid);

	std::string flowName = rootFlowNode["name"].as<std::string>();

	char uuidStr[37];
	uuid_unparse(_uuid, uuidStr);
	_logger->log_debug("parseRootProcessGroup: id => [%s]", uuidStr);
	_logger->log_debug("parseRootProcessGroup: name => [%s]", flowName.c_str());
	group = this->createRootProcessGroup(flowName, uuid);
	this->_root = group;
	this->_name = flowName;
}

void FlowController::parseProcessorNodeYaml(YAML::Node processorsNode, ProcessGroup *parentGroup) {
	int64_t schedulingPeriod = -1;
	int64_t penalizationPeriod = -1;
	int64_t yieldPeriod = -1;
	int64_t runDurationNanos = -1;
	uuid_t uuid;
	Processor *processor = NULL;

	if (!parentGroup) {
		_logger->log_error("parseProcessNodeYaml: no parent group exists");
		return;
	}

	if (processorsNode) {
		// Evaluate sequence of processors
		int numProcessors = processorsNode.size();
		if (numProcessors < 1) {
			throw new std::invalid_argument("There must be at least one processor configured.");
		}

		std::vector<ProcessorConfig> processorConfigs;

		if (processorsNode.IsSequence()) {
			for (YAML::const_iterator iter = processorsNode.begin(); iter != processorsNode.end(); ++iter) {
				ProcessorConfig procCfg;
				YAML::Node procNode = iter->as<YAML::Node>();

				procCfg.name = procNode["name"].as<std::string>();
				_logger->log_debug("parseProcessorNode: name => [%s]", procCfg.name.c_str());
				procCfg.javaClass = procNode["class"].as<std::string>();
				_logger->log_debug("parseProcessorNode: class => [%s]", procCfg.javaClass.c_str());

				char uuidStr[37];
				uuid_unparse(_uuid, uuidStr);

				// generate the random UUID
				uuid_generate(uuid);

				// Determine the processor name only from the Java class
				int lastOfIdx = procCfg.javaClass.find_last_of(".");
				if (lastOfIdx != std::string::npos) {
					lastOfIdx++; // if a value is found, increment to move beyond the .
					int nameLength = procCfg.javaClass.length() - lastOfIdx;
					std::string processorName = procCfg.javaClass.substr(lastOfIdx, nameLength);
					processor = this->createProcessor(processorName, uuid);
				}

				if (!processor) {
					_logger->log_error("Could not create a processor %s with name %s", procCfg.name.c_str(), uuidStr);
					throw std::invalid_argument("Could not create processor " + procCfg.name);
				}
				processor->setName(procCfg.name);

				procCfg.maxConcurrentTasks = procNode["max concurrent tasks"].as<std::string>();
				_logger->log_debug("parseProcessorNode: max concurrent tasks => [%s]", procCfg.maxConcurrentTasks.c_str());
				procCfg.schedulingStrategy = procNode["scheduling strategy"].as<std::string>();
				_logger->log_debug("parseProcessorNode: scheduling strategy => [%s]",
						procCfg.schedulingStrategy.c_str());
				procCfg.schedulingPeriod = procNode["scheduling period"].as<std::string>();
				_logger->log_debug("parseProcessorNode: scheduling period => [%s]", procCfg.schedulingPeriod.c_str());
				procCfg.penalizationPeriod = procNode["penalization period"].as<std::string>();
				_logger->log_debug("parseProcessorNode: penalization period => [%s]",
						procCfg.penalizationPeriod.c_str());
				procCfg.yieldPeriod = procNode["yield period"].as<std::string>();
				_logger->log_debug("parseProcessorNode: yield period => [%s]", procCfg.yieldPeriod.c_str());
				procCfg.yieldPeriod = procNode["run duration nanos"].as<std::string>();
				_logger->log_debug("parseProcessorNode: run duration nanos => [%s]", procCfg.runDurationNanos.c_str());

				// handle auto-terminated relationships
				YAML::Node autoTerminatedSequence = procNode["auto-terminated relationships list"];
				std::vector<std::string> rawAutoTerminatedRelationshipValues;
				if (autoTerminatedSequence.IsSequence() && !autoTerminatedSequence.IsNull()
						&& autoTerminatedSequence.size() > 0) {
					for (YAML::const_iterator relIter = autoTerminatedSequence.begin();
							relIter != autoTerminatedSequence.end(); ++relIter) {
						std::string autoTerminatedRel = relIter->as<std::string>();
						rawAutoTerminatedRelationshipValues.push_back(autoTerminatedRel);
					}
				}
				procCfg.autoTerminatedRelationships = rawAutoTerminatedRelationshipValues;

				// handle processor properties
				YAML::Node propertiesNode = procNode["Properties"];
				parsePropertiesNodeYaml(&propertiesNode, processor);

				// Take care of scheduling
				TimeUnit unit;
				if (Property::StringToTime(procCfg.schedulingPeriod, schedulingPeriod, unit)
						&& Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
					_logger->log_debug("convert: parseProcessorNode: schedulingPeriod => [%d] ns", schedulingPeriod);
					processor->setSchedulingPeriodNano(schedulingPeriod);
				}

				if (Property::StringToTime(procCfg.penalizationPeriod, penalizationPeriod, unit)
						&& Property::ConvertTimeUnitToMS(penalizationPeriod, unit, penalizationPeriod)) {
					_logger->log_debug("convert: parseProcessorNode: penalizationPeriod => [%d] ms",
							penalizationPeriod);
					processor->setPenalizationPeriodMsec(penalizationPeriod);
				}

				if (Property::StringToTime(procCfg.yieldPeriod, yieldPeriod, unit)
						&& Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod)) {
					_logger->log_debug("convert: parseProcessorNode: yieldPeriod => [%d] ms", yieldPeriod);
					processor->setYieldPeriodMsec(yieldPeriod);
				}

				// Default to running
				processor->setScheduledState(RUNNING);

				if (procCfg.schedulingStrategy == "TIMER_DRIVEN") {
					processor->setSchedulingStrategy(TIMER_DRIVEN);
					_logger->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy.c_str());
				} else if (procCfg.schedulingStrategy == "EVENT_DRIVEN") {
					processor->setSchedulingStrategy(EVENT_DRIVEN);
					_logger->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy.c_str());
				} else {
					processor->setSchedulingStrategy(CRON_DRIVEN);
					_logger->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy.c_str());

				}

				int64_t maxConcurrentTasks;
				if (Property::StringToInt(procCfg.maxConcurrentTasks, maxConcurrentTasks)) {
					_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
					processor->setMaxConcurrentTasks(maxConcurrentTasks);
				}

				if (Property::StringToInt(procCfg.runDurationNanos, runDurationNanos)) {
					_logger->log_debug("parseProcessorNode: runDurationNanos => [%d]", runDurationNanos);
					processor->setRunDurationNano(runDurationNanos);
				}

				std::set<Relationship> autoTerminatedRelationships;
				for (auto&& relString : procCfg.autoTerminatedRelationships) {
					Relationship relationship(relString, "");
					_logger->log_debug("parseProcessorNode: autoTerminatedRelationship  => [%s]", relString.c_str());
					autoTerminatedRelationships.insert(relationship);
				}

				processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

				parentGroup->addProcessor(processor);
			}
		}
	} else {
		throw new std::invalid_argument(
				"Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
	}
}

void FlowController::parseRemoteProcessGroupYaml(YAML::Node *rpgNode, ProcessGroup *parentGroup) {
	uuid_t uuid;

	if (!parentGroup) {
		_logger->log_error("parseRemoteProcessGroupYaml: no parent group exists");
		return;
	}

	if (rpgNode) {
		if (rpgNode->IsSequence()) {
			for (YAML::const_iterator iter = rpgNode->begin(); iter != rpgNode->end(); ++iter) {
				YAML::Node rpgNode = iter->as<YAML::Node>();

				auto name = rpgNode["name"].as<std::string>();
				_logger->log_debug("parseRemoteProcessGroupYaml: name => [%s]", name.c_str());

				std::string url = rpgNode["url"].as<std::string>();
				_logger->log_debug("parseRemoteProcessGroupYaml: url => [%s]", url.c_str());

				std::string timeout = rpgNode["timeout"].as<std::string>();
				_logger->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]", timeout.c_str());

				std::string yieldPeriod = rpgNode["yield period"].as<std::string>();
				_logger->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]", yieldPeriod.c_str());

				YAML::Node inputPorts = rpgNode["Input Ports"].as<YAML::Node>();

				if (inputPorts.IsSequence()) {
					for (YAML::const_iterator portIter = inputPorts.begin(); portIter != inputPorts.end(); ++portIter) {
						_logger->log_debug("Got a current port, iterating...");

						YAML::Node currPort = portIter->as<YAML::Node>();

						this->parsePortYaml(&currPort, parentGroup, SEND);

					} // for node
					char uuidStr[37];
					uuid_unparse(_uuid, uuidStr);

					// generate the random UUID
					uuid_generate(uuid);

					int64_t timeoutValue = -1;
					int64_t yieldPeriodValue = -1;

					ProcessGroup* group = this->createRemoteProcessGroup(name.c_str(), uuid);
					group->setParent(parentGroup);
					parentGroup->addProcessGroup(group);

					TimeUnit unit;

					if (Property::StringToTime(yieldPeriod, yieldPeriodValue, unit)
							&& Property::ConvertTimeUnitToMS(yieldPeriodValue, unit, yieldPeriodValue) && group) {
						_logger->log_debug("parseRemoteProcessGroup: yieldPeriod => [%d] ms", yieldPeriod.c_str());
						group->setYieldPeriodMsec(yieldPeriodValue);
					}

					if (Property::StringToTime(timeout, timeoutValue, unit)
							&& Property::ConvertTimeUnitToMS(timeoutValue, unit, timeoutValue) && group) {
						_logger->log_debug("parseRemoteProcessGroup: timeoutValue => [%d] ms", timeout.c_str());
						group->setTimeOut(yieldPeriodValue);
					}

					group->setTransmitting(true);
					group->setURL(url);

				}
			}
		}
	}
}

void FlowController::parseConnectionYaml(YAML::Node *connectionsNode, ProcessGroup *parent) {
	uuid_t uuid;
	Connection *connection = NULL;

	if (!parent) {
		_logger->log_error("parseProcessNode: no parent group was provided");
		return;
	}

	if (connectionsNode) {
		int numConnections = connectionsNode->size();
		if (numConnections < 1) {
			throw new std::invalid_argument("There must be at least one connection configured.");
		}

		if (connectionsNode->IsSequence()) {
			for (YAML::const_iterator iter = connectionsNode->begin(); iter != connectionsNode->end(); ++iter) {
				// generate the random UIID
				uuid_generate(uuid);

				YAML::Node connectionNode = iter->as<YAML::Node>();

				std::string name = connectionNode["name"].as<std::string>();
				std::string destName = connectionNode["destination name"].as<std::string>();

				char uuidStr[37];
				uuid_unparse(_uuid, uuidStr);

				_logger->log_debug("Created connection with UUID %s and name %s", uuidStr, name.c_str());
				connection = this->createConnection(name, uuid);
				auto rawRelationship = connectionNode["source relationship name"].as<std::string>();
				Relationship relationship(rawRelationship, "");
				_logger->log_debug("parseConnection: relationship => [%s]", rawRelationship.c_str());
				if (connection)
					connection->setRelationship(relationship);
				std::string connectionSrcProcName = connectionNode["source name"].as<std::string>();

				Processor *srcProcessor = this->_root->findProcessor(connectionSrcProcName);

				if (!srcProcessor) {
					_logger->log_error("Could not locate a source with name %s to create a connection",
							connectionSrcProcName.c_str());
					throw std::invalid_argument(
							"Could not locate a source with name %s to create a connection " + connectionSrcProcName);
				}

				Processor *destProcessor = this->_root->findProcessor(destName);
				// If we could not find name, try by UUID
				if (!destProcessor) {
					uuid_t destUuid;
					uuid_parse(destName.c_str(), destUuid);
					destProcessor = this->_root->findProcessor(destUuid);
				}
				if (destProcessor) {
					std::string destUuid = destProcessor->getUUIDStr();
				}

				uuid_t srcUuid;
				uuid_t destUuid;
				srcProcessor->getUUID(srcUuid);
				connection->setSourceProcessorUUID(srcUuid);
				destProcessor->getUUID(destUuid);
				connection->setDestinationProcessorUUID(destUuid);

				if (connection) {
					parent->addConnection(connection);
				}
			}
		}

		if (connection)
			parent->addConnection(connection);

		return;
	}
}

void FlowController::parseRemoteProcessGroup(xmlDoc *doc, xmlNode *node, ProcessGroup *parent) {
	uuid_t uuid;
	xmlNode *currentNode;
	ProcessGroup *group = NULL;
	int64_t yieldPeriod = -1;
	int64_t timeOut = -1;

// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
		if (currentNode->type == XML_ELEMENT_NODE) {
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0) {
				char *id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseRootProcessGroup: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseRemoteProcessGroup: name => [%s]", name);
					group = this->createRemoteProcessGroup(name, uuid);
					if (group == NULL) {
						xmlFree(name);
						return;
					}
					group->setParent(parent);
					parent->addProcessGroup(group);
					xmlFree(name);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "yieldPeriod") == 0) {
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToTime(temp, yieldPeriod, unit)
							&& Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod) && group) {
						_logger->log_debug("parseRemoteProcessGroup: yieldPeriod => [%d] ms", yieldPeriod);
						group->setYieldPeriodMsec(yieldPeriod);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "timeout") == 0) {
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToTime(temp, timeOut, unit)
							&& Property::ConvertTimeUnitToMS(timeOut, unit, timeOut) && group) {
						_logger->log_debug("parseRemoteProcessGroup: timeOut => [%d] ms", timeOut);
						group->setTimeOut(timeOut);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "transmitting") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				bool transmitting;
				if (temp) {
					if (Property::StringToBool(temp, transmitting) && group) {
						_logger->log_debug("parseRemoteProcessGroup: transmitting => [%d]", transmitting);
						group->setTransmitting(transmitting);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "inputPort") == 0 && group) {
				this->parsePort(doc, currentNode, group, SEND);
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "outputPort") == 0 && group) {
				this->parsePort(doc, currentNode, group, RECEIVE);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // for node
}

void FlowController::parseProcessorProperty(xmlDoc *doc, xmlNode *node, Processor *processor) {
	xmlNode *currentNode;
	std::string propertyValue;
	std::string propertyName;

	if (!processor) {
		_logger->log_error("parseProcessorProperty: no parent processor existed");
		return;
	}

	for (currentNode = node->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
		if (currentNode->type == XML_ELEMENT_NODE) {
			if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				char *name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					propertyName = name;
					xmlFree(name);
				}
			}
			if (xmlStrcmp(currentNode->name, BAD_CAST "value") == 0) {
				char *value = (char *) xmlNodeGetContent(currentNode);
				if (value) {
					_logger->log_debug("parseProcessorNode: value => [%s]", value);
					propertyValue = value;
					xmlFree(value);
				}
			}
			if (!propertyName.empty() && !propertyValue.empty()) {
				processor->setProperty(propertyName, propertyValue);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // for node
}

void FlowController::parsePortYaml(YAML::Node *portNode, ProcessGroup *parent, TransferDirection direction) {
	uuid_t uuid;
	Processor *processor = NULL;
	RemoteProcessorGroupPort *port = NULL;

	if (!parent) {
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}

	YAML::Node inputPortsObj = portNode->as<YAML::Node>();

	// generate the random UIID
	uuid_generate(uuid);

	auto portId = inputPortsObj["id"].as<std::string>();
	auto nameStr = inputPortsObj["name"].as<std::string>();
	uuid_parse(portId.c_str(), uuid);

	port = new RemoteProcessorGroupPort(nameStr.c_str(), uuid);

	processor = (Processor *) port;
	port->setDirection(direction);
	port->setTransmitting(true);
	processor->setYieldPeriodMsec(parent->getYieldPeriodMsec());
	processor->initialize();

	// handle port properties
	YAML::Node nodeVal = portNode->as<YAML::Node>();
	YAML::Node propertiesNode = nodeVal["Properties"];

	parsePropertiesNodeYaml(&propertiesNode, processor);

	// add processor to parent
	parent->addProcessor(processor);
	processor->setScheduledState(RUNNING);
	auto rawMaxConcurrentTasks = inputPortsObj["max concurrent tasks"].as<std::string>();
	int64_t maxConcurrentTasks;
	if (Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
		processor->setMaxConcurrentTasks(maxConcurrentTasks);
	}
	_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
	processor->setMaxConcurrentTasks(maxConcurrentTasks);

}

void FlowController::parsePort(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent, TransferDirection direction) {
	char *id = NULL;
	char *name = NULL;
	uuid_t uuid;
	xmlNode *currentNode;
	Processor *processor = NULL;
	RemoteProcessorGroupPort *port = NULL;

	if (!parent) {
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}
// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = processorNode->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
		if (currentNode->type == XML_ELEMENT_NODE) {
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0) {
				id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseProcessorNode: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					port = new RemoteProcessorGroupPort(name, uuid);
					processor = (Processor *) port;
					if (processor == NULL) {
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
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "scheduledState") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					std::string state = temp;
					if (state == "DISABLED") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(DISABLED);
					}
					if (state == "STOPPED") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(STOPPED);
					}
					if (state == "RUNNING") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(RUNNING);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxConcurrentTasks") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					int64_t maxConcurrentTasks;
					if (Property::StringToInt(temp, maxConcurrentTasks)) {
						_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
						processor->setMaxConcurrentTasks(maxConcurrentTasks);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "property") == 0) {
				this->parseProcessorProperty(doc, currentNode, processor);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // while node
}

void FlowController::parseProcessorNode(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent) {
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

	if (!parent) {
		_logger->log_error("parseProcessNode: no parent group existed");
		return;
	}
// generate the random UIID
	uuid_generate(uuid);

	for (currentNode = processorNode->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
		if (currentNode->type == XML_ELEMENT_NODE) {
			if (xmlStrcmp(currentNode->name, BAD_CAST "id") == 0) {
				id = (char *) xmlNodeGetContent(currentNode);
				if (id) {
					_logger->log_debug("parseProcessorNode: id => [%s]", id);
					uuid_parse(id, uuid);
					xmlFree(id);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "name") == 0) {
				name = (char *) xmlNodeGetContent(currentNode);
				if (name) {
					_logger->log_debug("parseProcessorNode: name => [%s]", name);
					processor = this->createProcessor(name, uuid);
					if (processor == NULL) {
						xmlFree(name);
						return;
					}
					// add processor to parent
					parent->addProcessor(processor);
					xmlFree(name);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "schedulingPeriod") == 0) {
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToTime(temp, schedulingPeriod, unit)
							&& Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
						_logger->log_debug("parseProcessorNode: schedulingPeriod => [%d] ns", schedulingPeriod);
						processor->setSchedulingPeriodNano(schedulingPeriod);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "penalizationPeriod") == 0) {
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToTime(temp, penalizationPeriod, unit)
							&& Property::ConvertTimeUnitToMS(penalizationPeriod, unit, penalizationPeriod)) {
						_logger->log_debug("parseProcessorNode: penalizationPeriod => [%d] ms", penalizationPeriod);
						processor->setPenalizationPeriodMsec(penalizationPeriod);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "yieldPeriod") == 0) {
				TimeUnit unit;
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToTime(temp, yieldPeriod, unit)
							&& Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod)) {
						_logger->log_debug("parseProcessorNode: yieldPeriod => [%d] ms", yieldPeriod);
						processor->setYieldPeriodMsec(yieldPeriod);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "lossTolerant") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToBool(temp, lossTolerant)) {
						_logger->log_debug("parseProcessorNode: lossTolerant => [%d]", lossTolerant);
						processor->setlossTolerant(lossTolerant);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "scheduledState") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					std::string state = temp;
					if (state == "DISABLED") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(DISABLED);
					}
					if (state == "STOPPED") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(STOPPED);
					}
					if (state == "RUNNING") {
						_logger->log_debug("parseProcessorNode: scheduledState  => [%s]", state.c_str());
						processor->setScheduledState(RUNNING);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "schedulingStrategy") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					std::string strategy = temp;
					if (strategy == "TIMER_DRIVEN") {
						_logger->log_debug("parseProcessorNode: scheduledStrategy  => [%s]", strategy.c_str());
						processor->setSchedulingStrategy(TIMER_DRIVEN);
					}
					if (strategy == "EVENT_DRIVEN") {
						_logger->log_debug("parseProcessorNode: scheduledStrategy  => [%s]", strategy.c_str());
						processor->setSchedulingStrategy(EVENT_DRIVEN);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxConcurrentTasks") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					int64_t maxConcurrentTasks;
					if (Property::StringToInt(temp, maxConcurrentTasks)) {
						_logger->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
						processor->setMaxConcurrentTasks(maxConcurrentTasks);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "runDurationNanos") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					if (Property::StringToInt(temp, runDurationNanos)) {
						_logger->log_debug("parseProcessorNode: runDurationNanos => [%d]", runDurationNanos);
						processor->setRunDurationNano(runDurationNanos);
					}
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "autoTerminatedRelationship") == 0) {
				char *temp = (char *) xmlNodeGetContent(currentNode);
				if (temp) {
					std::string relationshipName = temp;
					Relationship relationship(relationshipName, "");
					std::set<Relationship> relationships;

					relationships.insert(relationship);
					processor->setAutoTerminatedRelationships(relationships);
					_logger->log_debug("parseProcessorNode: autoTerminatedRelationship  => [%s]",
							relationshipName.c_str());
					xmlFree(temp);
				}
			} else if (xmlStrcmp(currentNode->name, BAD_CAST "property") == 0) {
				this->parseProcessorProperty(doc, currentNode, processor);
			}
		} // if (currentNode->type == XML_ELEMENT_NODE)
	} // while node
}

void FlowController::parsePropertiesNodeYaml(YAML::Node *propertiesNode, Processor *processor)
{
    // Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
    for (YAML::const_iterator propsIter = propertiesNode->begin(); propsIter != propertiesNode->end(); ++propsIter)
    {
        std::string propertyName = propsIter->first.as<std::string>();
        YAML::Node propertyValueNode = propsIter->second;
        if (!propertyValueNode.IsNull() && propertyValueNode.IsDefined())
        {
            std::string rawValueString = propertyValueNode.as<std::string>();
            if (!processor->setProperty(propertyName, rawValueString))
            {
                _logger->log_warn("Received property %s with value %s but is not one of the properties for %s", propertyName.c_str(), rawValueString.c_str(), processor->getName().c_str());
            }
        }
    }
}

void FlowController::load(ConfigFormat configFormat) {
	if (_running) {
		stop(true);
	}
	if (!_initialized) {
		_logger->log_info("Load Flow Controller from file %s", _configurationFileName.c_str());

		if (ConfigFormat::XML == configFormat) {
			_logger->log_info("Detected an XML configuration file for processing.");

			xmlDoc *doc = xmlReadFile(_configurationFileName.c_str(), NULL, XML_PARSE_NONET);
			if (doc == NULL) {
				_logger->log_error("xmlReadFile returned NULL when reading [%s]", _configurationFileName.c_str());
				_initialized = true;
				return;
			}

			xmlNode *root = xmlDocGetRootElement(doc);

			if (root == NULL) {
				_logger->log_error("Can not get root from XML doc %s", _configurationFileName.c_str());
				xmlFreeDoc(doc);
				xmlCleanupParser();
			}

			if (xmlStrcmp(root->name, BAD_CAST "flowController") != 0) {
				_logger->log_error("Root name is not flowController for XML doc %s", _configurationFileName.c_str());
				xmlFreeDoc(doc);
				xmlCleanupParser();
				return;
			}

			xmlNode *currentNode;

			for (currentNode = root->xmlChildrenNode; currentNode != NULL; currentNode = currentNode->next) {
				if (currentNode->type == XML_ELEMENT_NODE) {
					if (xmlStrcmp(currentNode->name, BAD_CAST "rootGroup") == 0) {
						this->parseRootProcessGroup(doc, currentNode);
					} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxTimerDrivenThreadCount") == 0) {
						char *temp = (char *) xmlNodeGetContent(currentNode);
						int64_t maxTimerDrivenThreadCount;
						if (temp) {
							if (Property::StringToInt(temp, maxTimerDrivenThreadCount)) {
								_logger->log_debug("maxTimerDrivenThreadCount => [%d]", maxTimerDrivenThreadCount);
								this->_maxTimerDrivenThreads = maxTimerDrivenThreadCount;
							}
							xmlFree(temp);
						}
					} else if (xmlStrcmp(currentNode->name, BAD_CAST "maxEventDrivenThreadCount") == 0) {
						char *temp = (char *) xmlNodeGetContent(currentNode);
						int64_t maxEventDrivenThreadCount;
						if (temp) {
							if (Property::StringToInt(temp, maxEventDrivenThreadCount)) {
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
		} else if (ConfigFormat::YAML == configFormat) {
			YAML::Node flow = YAML::LoadFile(_configurationFileName);

			YAML::Node flowControllerNode = flow["Flow Controller"];
			YAML::Node processorsNode = flow[CONFIG_YAML_PROCESSORS_KEY];
			YAML::Node connectionsNode = flow["Connections"];
			YAML::Node remoteProcessingGroupNode = flow["Remote Processing Groups"];

			// Create the root process group
			parseRootProcessGroupYaml(flowControllerNode);
			parseProcessorNodeYaml(processorsNode, this->_root);
			parseRemoteProcessGroupYaml(&remoteProcessingGroupNode, this->_root);
			parseConnectionYaml(&connectionsNode, this->_root);

			_initialized = true;
		}
	}
}

bool FlowController::start() {
	if (!_initialized) {
		_logger->log_error("Can not start Flow Controller because it has not been initialized");
		return false;
	} else {
		if (!_running) {
			_logger->log_info("Start Flow Controller");
			this->_timerScheduler.start();
			if (this->_root)
				this->_root->startProcessing(&this->_timerScheduler);
			_running = true;
		}
		return true;
	}
}
