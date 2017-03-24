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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <future>
#include "FlowController.h"
#include "ProcessContext.h"
#include "utils/StringUtils.h"

FlowController *FlowControllerFactory::_flowController(NULL);

FlowControllerImpl::FlowControllerImpl(std::string name)  {
	uuid_generate(_uuid);

	_name = name;
	// Setup the default values
	_configurationFileName = DEFAULT_FLOW_YAML_FILE_NAME;
	_maxEventDrivenThreads = DEFAULT_MAX_EVENT_DRIVEN_THREAD;
	_maxTimerDrivenThreads = DEFAULT_MAX_TIMER_DRIVEN_THREAD;
	_running = false;
	_initialized = false;
	_root = NULL;
	logger_ = Logger::getLogger();
	_protocol = new FlowControlProtocol(this);

	// NiFi config properties
	configure_ = Configure::getConfigure();

	std::string rawConfigFileString;
	configure_->get(Configure::nifi_flow_configuration_file,
			rawConfigFileString);

	if (!rawConfigFileString.empty()) {
		_configurationFileName = rawConfigFileString;
	}

	char *path = NULL;
	char full_path[PATH_MAX];

	std::string adjustedFilename;
	if (!_configurationFileName.empty()) {
		// perform a naive determination if this is a relative path
		if (_configurationFileName.c_str()[0] != '/') {
			adjustedFilename = adjustedFilename + configure_->getHome() + "/"
					+ _configurationFileName;
		} else {
			adjustedFilename = _configurationFileName;
		}
	}

	path = realpath(adjustedFilename.c_str(), full_path);

	std::string pathString(path);
	_configurationFileName = pathString;
	logger_->log_info("FlowController NiFi Configuration file %s", pathString.c_str());

	// Create the content repo directory if needed
	struct stat contentDirStat;

	if (stat(ResourceClaim::default_directory_path.c_str(), &contentDirStat) != -1 && S_ISDIR(contentDirStat.st_mode))
	{
		path = realpath(ResourceClaim::default_directory_path.c_str(), full_path);
		logger_->log_info("FlowController content directory %s", full_path);
	}
	else
	{
	   if (mkdir(ResourceClaim::default_directory_path.c_str(), 0777) == -1)
	   {
		   logger_->log_error("FlowController content directory creation failed");
		   exit(1);
	   }
	}

	
	std::string clientAuthStr;

	if (!path) {
		logger_->log_error(
				"Could not locate path from provided configuration file name (%s).  Exiting.",
				full_path);
		exit(1);
	}


	// Create repos for flow record and provenance
	_flowfileRepo = new FlowFileRepository();
	_flowfileRepo->initialize();
	_provenanceRepo = new ProvenanceRepository();
	_provenanceRepo->initialize();
}

FlowControllerImpl::~FlowControllerImpl() {

	stop(true);
	unload();
	if (NULL != _protocol)
		delete _protocol;
	if (NULL != _provenanceRepo)
		delete _provenanceRepo;
	if (NULL != _flowfileRepo)
		delete _flowfileRepo;

}

void FlowControllerImpl::stop(bool force) {

	if (_running) {
		// immediately indicate that we are not running
		_running = false;

		logger_->log_info("Stop Flow Controller");
		this->_timerScheduler.stop();
		this->_eventScheduler.stop();
		this->_flowfileRepo->stop();
		this->_provenanceRepo->stop();
		// Wait for sometime for thread stop
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		if (this->_root)
			this->_root->stopProcessing(&this->_timerScheduler,
					&this->_eventScheduler);

	}
}

/**
 * This function will attempt to unload yaml and stop running Processors.
 *
 * If the latter attempt fails or does not complete within the prescribed
 * period, _running will be set to false and we will return.
 *
 * @param timeToWaitMs Maximum time to wait before manually
 * marking running as false.
 */
void FlowControllerImpl::waitUnload(const uint64_t timeToWaitMs) {
	if (_running) {
		// use the current time and increment with the provided argument.
		std::chrono::system_clock::time_point wait_time =
				std::chrono::system_clock::now()
						+ std::chrono::milliseconds(timeToWaitMs);

		// create an asynchronous future.
		std::future<void> unload_task = std::async(std::launch::async,
				[this]() {unload();});

		if (std::future_status::ready == unload_task.wait_until(wait_time)) {
			_running = false;
		}

	}
}


void FlowControllerImpl::unload() {
	if (_running) {
		stop(true);
	}
	if (_initialized) {
		logger_->log_info("Unload Flow Controller");
		if (_root)
			delete _root;
		_root = NULL;
		_initialized = false;
		_name = "";
	}

	return;
}

Processor *FlowControllerImpl::createProcessor(std::string name, uuid_t uuid) {
	Processor *processor = NULL;
	if (name == GenerateFlowFile::ProcessorName) {
		processor = new GenerateFlowFile(name, uuid);
	} else if (name == LogAttribute::ProcessorName) {
		processor = new LogAttribute(name, uuid);
	} else if (name == RealTimeDataCollector::ProcessorName) {
		processor = new RealTimeDataCollector(name, uuid);
	} else if (name == GetFile::ProcessorName) {
		processor = new GetFile(name, uuid);
	} else if (name == PutFile::ProcessorName) {
		processor = new PutFile(name, uuid);
	} else if (name == TailFile::ProcessorName) {
		processor = new TailFile(name, uuid);
	} else if (name == ListenSyslog::ProcessorName) {
		processor = new ListenSyslog(name, uuid);
	} else if (name == ListenHTTP::ProcessorName) {
        processor = new ListenHTTP(name, uuid);
	} else if (name == ExecuteProcess::ProcessorName) {
		processor = new ExecuteProcess(name, uuid);
	} else if (name == AppendHostInfo::ProcessorName) {
		processor = new AppendHostInfo(name, uuid);
	} else {
		logger_->log_error("No Processor defined for %s", name.c_str());
		return NULL;
	}

	//! initialize the processor
	processor->initialize();

	return processor;
}

ProcessGroup *FlowControllerImpl::createRootProcessGroup(std::string name,
		uuid_t uuid) {
	return new ProcessGroup(ROOT_PROCESS_GROUP, name, uuid);
}

ProcessGroup *FlowControllerImpl::createRemoteProcessGroup(std::string name,
		uuid_t uuid) {
	return new ProcessGroup(REMOTE_PROCESS_GROUP, name, uuid);
}

Connection *FlowControllerImpl::createConnection(std::string name,
		uuid_t uuid) {
	return new Connection(name, uuid);
}

#ifdef YAML_SUPPORT
void FlowControllerImpl::parseRootProcessGroupYaml(YAML::Node rootFlowNode) {
	uuid_t uuid;
	ProcessGroup *group = NULL;

	std::string flowName = rootFlowNode["name"].as<std::string>();
	std::string id = rootFlowNode["id"].as<std::string>();

	uuid_parse(id.c_str(), uuid);

	logger_->log_debug("parseRootProcessGroup: id => [%s]", id.c_str());
	logger_->log_debug("parseRootProcessGroup: name => [%s]", flowName.c_str());
	group = this->createRootProcessGroup(flowName, uuid);
	this->_root = group;
	this->_name = flowName;
}

void FlowControllerImpl::parseProcessorNodeYaml(YAML::Node processorsNode,
		ProcessGroup *parentGroup) {
	int64_t schedulingPeriod = -1;
	int64_t penalizationPeriod = -1;
	int64_t yieldPeriod = -1;
	int64_t runDurationNanos = -1;
	uuid_t uuid;
	Processor *processor = NULL;

	if (!parentGroup) {
		logger_->log_error("parseProcessNodeYaml: no parent group exists");
		return;
	}

	if (processorsNode) {

		if (processorsNode.IsSequence()) {
			// Evaluate sequence of processors
			int numProcessors = processorsNode.size();

			for (YAML::const_iterator iter = processorsNode.begin();
					iter != processorsNode.end(); ++iter) {
				ProcessorConfig procCfg;
				YAML::Node procNode = iter->as<YAML::Node>();

				procCfg.name = procNode["name"].as<std::string>();
				procCfg.id = procNode["id"].as<std::string>();
				logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]",
						procCfg.name.c_str(), procCfg.id.c_str());
				procCfg.javaClass = procNode["class"].as<std::string>();
				logger_->log_debug("parseProcessorNode: class => [%s]",
						procCfg.javaClass.c_str());

				uuid_parse(procCfg.id.c_str(), uuid);

				// Determine the processor name only from the Java class
				int lastOfIdx = procCfg.javaClass.find_last_of(".");
				if (lastOfIdx != std::string::npos) {
					lastOfIdx++; // if a value is found, increment to move beyond the .
					int nameLength = procCfg.javaClass.length() - lastOfIdx;
					std::string processorName = procCfg.javaClass.substr(
							lastOfIdx, nameLength);
					processor = this->createProcessor(processorName, uuid);
				}

				if (!processor) {
					logger_->log_error(
							"Could not create a processor %s with name %s",
							procCfg.name.c_str(), procCfg.id.c_str());
					throw std::invalid_argument(
							"Could not create processor " + procCfg.name);
				}
				processor->setName(procCfg.name);

				procCfg.maxConcurrentTasks =
						procNode["max concurrent tasks"].as<std::string>();
				logger_->log_debug(
						"parseProcessorNode: max concurrent tasks => [%s]",
						procCfg.maxConcurrentTasks.c_str());
				procCfg.schedulingStrategy = procNode["scheduling strategy"].as<
						std::string>();
				logger_->log_debug(
						"parseProcessorNode: scheduling strategy => [%s]",
						procCfg.schedulingStrategy.c_str());
				procCfg.schedulingPeriod = procNode["scheduling period"].as<
						std::string>();
				logger_->log_debug(
						"parseProcessorNode: scheduling period => [%s]",
						procCfg.schedulingPeriod.c_str());
				procCfg.penalizationPeriod = procNode["penalization period"].as<
						std::string>();
				logger_->log_debug(
						"parseProcessorNode: penalization period => [%s]",
						procCfg.penalizationPeriod.c_str());
				procCfg.yieldPeriod =
						procNode["yield period"].as<std::string>();
				logger_->log_debug("parseProcessorNode: yield period => [%s]",
						procCfg.yieldPeriod.c_str());
				procCfg.yieldPeriod = procNode["run duration nanos"].as<
						std::string>();
				logger_->log_debug(
						"parseProcessorNode: run duration nanos => [%s]",
						procCfg.runDurationNanos.c_str());

				// handle auto-terminated relationships
				YAML::Node autoTerminatedSequence =
						procNode["auto-terminated relationships list"];
				std::vector<std::string> rawAutoTerminatedRelationshipValues;
				if (autoTerminatedSequence.IsSequence()
						&& !autoTerminatedSequence.IsNull()
						&& autoTerminatedSequence.size() > 0) {
					for (YAML::const_iterator relIter =
							autoTerminatedSequence.begin();
							relIter != autoTerminatedSequence.end();
							++relIter) {
						std::string autoTerminatedRel =
								relIter->as<std::string>();
						rawAutoTerminatedRelationshipValues.push_back(
								autoTerminatedRel);
					}
				}
				procCfg.autoTerminatedRelationships =
						rawAutoTerminatedRelationshipValues;

				// handle processor properties
				YAML::Node propertiesNode = procNode["Properties"];
				parsePropertiesNodeYaml(&propertiesNode, processor);

				// Take care of scheduling
				TimeUnit unit;
				if (Property::StringToTime(procCfg.schedulingPeriod,
						schedulingPeriod, unit)
						&& Property::ConvertTimeUnitToNS(schedulingPeriod, unit,
								schedulingPeriod)) {
					logger_->log_debug(
							"convert: parseProcessorNode: schedulingPeriod => [%d] ns",
							schedulingPeriod);
					processor->setSchedulingPeriodNano(schedulingPeriod);
				}

				if (Property::StringToTime(procCfg.penalizationPeriod,
						penalizationPeriod, unit)
						&& Property::ConvertTimeUnitToMS(penalizationPeriod,
								unit, penalizationPeriod)) {
					logger_->log_debug(
							"convert: parseProcessorNode: penalizationPeriod => [%d] ms",
							penalizationPeriod);
					processor->setPenalizationPeriodMsec(penalizationPeriod);
				}

				if (Property::StringToTime(procCfg.yieldPeriod, yieldPeriod,
						unit)
						&& Property::ConvertTimeUnitToMS(yieldPeriod, unit,
								yieldPeriod)) {
					logger_->log_debug(
							"convert: parseProcessorNode: yieldPeriod => [%d] ms",
							yieldPeriod);
					processor->setYieldPeriodMsec(yieldPeriod);
				}

				// Default to running
				processor->setScheduledState(RUNNING);

				if (procCfg.schedulingStrategy == "TIMER_DRIVEN") {
					processor->setSchedulingStrategy(TIMER_DRIVEN);
					logger_->log_debug("setting scheduling strategy as %s",
							procCfg.schedulingStrategy.c_str());
				} else if (procCfg.schedulingStrategy == "EVENT_DRIVEN") {
					processor->setSchedulingStrategy(EVENT_DRIVEN);
					logger_->log_debug("setting scheduling strategy as %s",
							procCfg.schedulingStrategy.c_str());
				} else {
					processor->setSchedulingStrategy(CRON_DRIVEN);
					logger_->log_debug("setting scheduling strategy as %s",
							procCfg.schedulingStrategy.c_str());

				}

				int64_t maxConcurrentTasks;
				if (Property::StringToInt(procCfg.maxConcurrentTasks,
						maxConcurrentTasks)) {
					logger_->log_debug(
							"parseProcessorNode: maxConcurrentTasks => [%d]",
							maxConcurrentTasks);
					processor->setMaxConcurrentTasks(maxConcurrentTasks);
				}

				if (Property::StringToInt(procCfg.runDurationNanos,
						runDurationNanos)) {
					logger_->log_debug(
							"parseProcessorNode: runDurationNanos => [%d]",
							runDurationNanos);
					processor->setRunDurationNano(runDurationNanos);
				}

				std::set<Relationship> autoTerminatedRelationships;
				for (auto &&relString : procCfg.autoTerminatedRelationships) {
					Relationship relationship(relString, "");
					logger_->log_debug(
							"parseProcessorNode: autoTerminatedRelationship  => [%s]",
							relString.c_str());
					autoTerminatedRelationships.insert(relationship);
				}

				processor->setAutoTerminatedRelationships(
						autoTerminatedRelationships);

				parentGroup->addProcessor(processor);
			}
		}
	} else {
		throw new std::invalid_argument(
				"Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
	}
}

void FlowControllerImpl::parseRemoteProcessGroupYaml(YAML::Node *rpgNode,
		ProcessGroup *parentGroup) {
	uuid_t uuid;

	if (!parentGroup) {
		logger_->log_error(
				"parseRemoteProcessGroupYaml: no parent group exists");
		return;
	}

	if (rpgNode) {
		if (rpgNode->IsSequence()) {
			for (YAML::const_iterator iter = rpgNode->begin();
					iter != rpgNode->end(); ++iter) {
				YAML::Node rpgNode = iter->as<YAML::Node>();

				auto name = rpgNode["name"].as<std::string>();
				auto id = rpgNode["id"].as<std::string>();

				logger_->log_debug("parseRemoteProcessGroupYaml: name => [%s], id => [%s]",
						name.c_str(), id.c_str());

				std::string url = rpgNode["url"].as<std::string>();
				logger_->log_debug("parseRemoteProcessGroupYaml: url => [%s]",
						url.c_str());

				std::string timeout = rpgNode["timeout"].as<std::string>();
				logger_->log_debug(
						"parseRemoteProcessGroupYaml: timeout => [%s]",
						timeout.c_str());

				std::string yieldPeriod =
						rpgNode["yield period"].as<std::string>();
				logger_->log_debug(
						"parseRemoteProcessGroupYaml: yield period => [%s]",
						yieldPeriod.c_str());

				YAML::Node inputPorts = rpgNode["Input Ports"].as<YAML::Node>();
				YAML::Node outputPorts =
						rpgNode["Output Ports"].as<YAML::Node>();
				ProcessGroup *group = NULL;

				uuid_parse(id.c_str(), uuid);

				int64_t timeoutValue = -1;
				int64_t yieldPeriodValue = -1;

				group = this->createRemoteProcessGroup(name.c_str(), uuid);
				group->setParent(parentGroup);
				parentGroup->addProcessGroup(group);

				TimeUnit unit;

				if (Property::StringToTime(yieldPeriod, yieldPeriodValue, unit)
						&& Property::ConvertTimeUnitToMS(yieldPeriodValue, unit,
								yieldPeriodValue) && group) {
					logger_->log_debug(
							"parseRemoteProcessGroupYaml: yieldPeriod => [%d] ms",
							yieldPeriodValue);
					group->setYieldPeriodMsec(yieldPeriodValue);
				}

				if (Property::StringToTime(timeout, timeoutValue, unit)
						&& Property::ConvertTimeUnitToMS(timeoutValue, unit,
								timeoutValue) && group) {
					logger_->log_debug(
							"parseRemoteProcessGroupYaml: timeoutValue => [%d] ms",
							timeoutValue);
					group->setTimeOut(timeoutValue);
				}

				group->setTransmitting(true);
				group->setURL(url);

				if (inputPorts && inputPorts.IsSequence()) {
					for (YAML::const_iterator portIter = inputPorts.begin();
							portIter != inputPorts.end(); ++portIter) {
						logger_->log_debug("Got a current port, iterating...");

						YAML::Node currPort = portIter->as<YAML::Node>();

						this->parsePortYaml(&currPort, group, SEND);
					} // for node
				}
				if (outputPorts && outputPorts.IsSequence()) {
					for (YAML::const_iterator portIter = outputPorts.begin();
							portIter != outputPorts.end(); ++portIter) {
						logger_->log_debug("Got a current port, iterating...");

						YAML::Node currPort = portIter->as<YAML::Node>();

						this->parsePortYaml(&currPort, group, RECEIVE);
					} // for node
				}

			}
		}
	}
}

void FlowControllerImpl::parseConnectionYaml(YAML::Node *connectionsNode,
		ProcessGroup *parent) {
	uuid_t uuid;
	Connection *connection = NULL;

	if (!parent) {
		logger_->log_error("parseProcessNode: no parent group was provided");
		return;
	}

	if (connectionsNode) {

		if (connectionsNode->IsSequence()) {
			for (YAML::const_iterator iter = connectionsNode->begin();
					iter != connectionsNode->end(); ++iter) {

				YAML::Node connectionNode = iter->as<YAML::Node>();

				std::string name = connectionNode["name"].as<std::string>();
				std::string id = connectionNode["id"].as<std::string>();
				std::string destId = connectionNode["destination id"].as<
						std::string>();

				uuid_parse(id.c_str(), uuid);

				logger_->log_debug(
						"Created connection with UUID %s and name %s", id.c_str(),
						name.c_str());
				connection = this->createConnection(name, uuid);
				auto rawRelationship =
						connectionNode["source relationship name"].as<
								std::string>();
				Relationship relationship(rawRelationship, "");
				logger_->log_debug("parseConnection: relationship => [%s]",
						rawRelationship.c_str());
				if (connection)
					connection->setRelationship(relationship);
				std::string connectionSrcProcId =
						connectionNode["source id"].as<std::string>();
				uuid_t srcUUID;
				uuid_parse(connectionSrcProcId.c_str(), srcUUID);

				Processor *srcProcessor = this->_root->findProcessor(
						srcUUID);

				if (!srcProcessor) {
					logger_->log_error(
							"Could not locate a source with id %s to create a connection",
							connectionSrcProcId.c_str());
					throw std::invalid_argument(
							"Could not locate a source with id %s to create a connection "
									+ connectionSrcProcId);
				}

				uuid_t destUUID;
				uuid_parse(destId.c_str(), destUUID);
				Processor *destProcessor = this->_root->findProcessor(destUUID);
				// If we could not find name, try by UUID
				if (!destProcessor) {
					uuid_t destUuid;
					uuid_parse(destId.c_str(), destUuid);
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

void FlowControllerImpl::parsePortYaml(YAML::Node *portNode,
		ProcessGroup *parent, TransferDirection direction) {
	uuid_t uuid;
	Processor *processor = NULL;
	RemoteProcessorGroupPort *port = NULL;

	if (!parent) {
		logger_->log_error("parseProcessNode: no parent group existed");
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
	port->setTimeOut(parent->getTimeOut());
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
	auto rawMaxConcurrentTasks = inputPortsObj["max concurrent tasks"].as<
			std::string>();
	int64_t maxConcurrentTasks;
	if (Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
		processor->setMaxConcurrentTasks(maxConcurrentTasks);
	}
	logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]",
			maxConcurrentTasks);
	processor->setMaxConcurrentTasks(maxConcurrentTasks);

}

void FlowControllerImpl::parsePropertiesNodeYaml(YAML::Node *propertiesNode,
		Processor *processor) {
	// Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
	for (YAML::const_iterator propsIter = propertiesNode->begin();
			propsIter != propertiesNode->end(); ++propsIter) {
		std::string propertyName = propsIter->first.as<std::string>();
		YAML::Node propertyValueNode = propsIter->second;
		if (!propertyValueNode.IsNull() && propertyValueNode.IsDefined()) {
			std::string rawValueString = propertyValueNode.as<std::string>();
			if (!processor->setProperty(propertyName, rawValueString)) {
				logger_->log_warn(
						"Received property %s with value %s but is not one of the properties for %s",
						propertyName.c_str(), rawValueString.c_str(),
						processor->getName().c_str());
			}
		}
	}
}
#endif /* ifdef YAML_SUPPORT */

void FlowControllerImpl::load() {
    if (_running) {
        stop(true);
    }
    if (!_initialized) {
        logger_->log_info("Load Flow Controller from file %s", _configurationFileName.c_str());

#ifdef YAML_SUPPORT
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

		// Load Flow File from Repo
		loadFlowRepo();
#endif

		_initialized = true;
    }
}

void FlowControllerImpl::loadFlowRepo()
{
	if (this->_flowfileRepo && this->_flowfileRepo->isEnable())
	{
		std::map<std::string, Connection *> connectionMap;
		this->_root->getConnections(&connectionMap);
		this->_flowfileRepo->loadFlowFileToConnections(&connectionMap);
	}
}

void FlowControllerImpl::reload(std::string yamlFile)
{
    logger_->log_info("Starting to reload Flow Controller with yaml %s", yamlFile.c_str());
    stop(true);
    unload();
    std::string oldYamlFile = this->_configurationFileName;
    this->_configurationFileName = yamlFile;
    load();
    start();
       if (!this->_root)
       {
        this->_configurationFileName = oldYamlFile;
        logger_->log_info("Rollback Flow Controller to YAML %s", oldYamlFile.c_str());
        stop(true);
        unload();
        load();
        start();
    }
}

bool FlowControllerImpl::start() {
	if (!_initialized) {
		logger_->log_error(
				"Can not start Flow Controller because it has not been initialized");
		return false;
	} else {

		if (!_running) {
			logger_->log_info("Starting Flow Controller");
			this->_timerScheduler.start();
			this->_eventScheduler.start();
			if (this->_root)
				this->_root->startProcessing(&this->_timerScheduler,
						&this->_eventScheduler);
			_running = true;
			this->_protocol->start();
			this->_provenanceRepo->start();
			this->_flowfileRepo->start();
			logger_->log_info("Started Flow Controller");
		}
		return true;
	}
}
