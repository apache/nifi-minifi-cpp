/**
 * @file FlowController.h
 * FlowController class declaration
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
#ifndef __FLOW_CONTROLLER_H__
#define __FLOW_CONTROLLER_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <yaml-cpp/yaml.h>

#include "Configure.h"
#include "Property.h"
#include "Relationship.h"
#include "FlowFileRecord.h"
#include "Connection.h"
#include "Processor.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessGroup.h"
#include "GenerateFlowFile.h"
#include "LogAttribute.h"
#include "RealTimeDataCollector.h"
#include "TimerDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"
#include "RemoteProcessorGroupPort.h"
#include "Provenance.h"
#include "GetFile.h"
#include "TailFile.h"
#include "ListenSyslog.h"
#include "ExecuteProcess.h"

//! Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""
#define DEFAULT_FLOW_XML_FILE_NAME "conf/flow.xml"
#define DEFAULT_FLOW_YAML_FILE_NAME "conf/flow.yml"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"

enum class ConfigFormat { XML, YAML };

struct ProcessorConfig {
	std::string name;
	std::string javaClass;
	std::string maxConcurrentTasks;
	std::string schedulingStrategy;
	std::string schedulingPeriod;
	std::string penalizationPeriod;
	std::string yieldPeriod;
	std::string runDurationNanos;
	std::vector<std::string> autoTerminatedRelationships;
	std::vector<Property> properties;
};

//! FlowController Class
class FlowController
{
public:
    static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
    static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;
	//! Get the singleton flow controller
	static FlowController * getFlowController()
	{
		if (!_flowController)
		{
			_flowController = new FlowController();
		}
		return _flowController;
	}

	//! Destructor
	virtual ~FlowController();
	//! Set FlowController Name
	void setName(std::string name) {
		_name = name;
	}
	//! Get Flow Controller Name
	std::string getName(void) {
		return (_name);
	}
	//! Set UUID
	void setUUID(uuid_t uuid) {
		uuid_copy(_uuid, uuid);
	}
	//! Get UUID
	bool getUUID(uuid_t uuid) {
		if (uuid)
		{
			uuid_copy(uuid, _uuid);
			return true;
		}
		else
			return false;
	}
	//! Set MAX TimerDrivenThreads
	void setMaxTimerDrivenThreads(int number)
	{
		_maxTimerDrivenThreads = number;
	}
	//! Get MAX TimerDrivenThreads
	int getMaxTimerDrivenThreads()
	{
		return _maxTimerDrivenThreads;
	}
	//! Set MAX EventDrivenThreads
	void setMaxEventDrivenThreads(int number)
	{
		_maxEventDrivenThreads = number;
	}
	//! Get MAX EventDrivenThreads
	int getMaxEventDrivenThreads()
	{
		return _maxEventDrivenThreads;
	}
	//! Get the provenance repository
	ProvenanceRepository *getProvenanceRepository()
	{
		return this->_provenanceRepo;
	}
	//! Life Cycle related function
	//! Load flow xml from disk, after that, create the root process group and its children, initialize the flows
	void load(ConfigFormat format);
	//! Whether the Flow Controller is start running
	bool isRunning();
	//! Whether the Flow Controller has already been initialized (loaded flow XML)
	bool isInitialized();
	//! Start to run the Flow Controller which internally start the root process group and all its children
	bool start();
	//! Stop to run the Flow Controller which internally stop the root process group and all its children
	void stop(bool force);
	//! Unload the current flow xml, clean the root process group and all its children
	void unload();
	//! Load new xml
	void reload(std::string xmlFile);
	//! update property value
	void updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue)
	{
		if (_root)
			_root->updatePropertyValue(processorName, propertyName, propertyValue);
	}

	//! Create Processor (Node/Input/Output Port) based on the name
	Processor *createProcessor(std::string name, uuid_t uuid);
	//! Create Root Processor Group
	ProcessGroup *createRootProcessGroup(std::string name, uuid_t uuid);
	//! Create Remote Processor Group
	ProcessGroup *createRemoteProcessGroup(std::string name, uuid_t uuid);
	//! Create Connection
	Connection *createConnection(std::string name, uuid_t uuid);
	//! set 8 bytes SerialNumber
	void setSerialNumber(uint8_t *number)
	{
		_protocol->setSerialNumber(number);
	}

protected:

	//! A global unique identifier
	uuid_t _uuid;
	//! FlowController Name
	std::string _name;
	//! Configuration File Name
	std::string _configurationFileName;
	//! NiFi property File Name
	std::string _propertiesFileName;
	//! Root Process Group
	ProcessGroup *_root;
	//! MAX Timer Driven Threads
	int _maxTimerDrivenThreads;
	//! MAX Event Driven Threads
	int _maxEventDrivenThreads;
	//! Config
	//! FlowFile Repo
	//! Provenance Repo
	ProvenanceRepository *_provenanceRepo;
	//! Flow Engines
	//! Flow Scheduler
	TimerDrivenSchedulingAgent _timerScheduler;
	//! Controller Service
	//! Config
	//! Site to Site Server Listener
	//! Heart Beat
	//! FlowControl Protocol
	FlowControlProtocol *_protocol;

private:

	//! Mutex for protection
	std::mutex _mtx;
	//! Logger
	Logger *_logger;
	//! Configure
	Configure *_configure;
	//! Whether it is running
	std::atomic<bool> _running;
	//! Whether it has already been initialized (load the flow XML already)
	std::atomic<bool> _initialized;
	//! Process Processor Node XML
	void parseProcessorNode(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent);
	//! Process Port XML
	void parsePort(xmlDoc *doc, xmlNode *processorNode, ProcessGroup *parent, TransferDirection direction);
	//! Process Root Processor Group XML
	void parseRootProcessGroup(xmlDoc *doc, xmlNode *node);
	//! Process Property XML
	void parseProcessorProperty(xmlDoc *doc, xmlNode *node, Processor *processor);
	//! Process connection XML
	void parseConnection(xmlDoc *doc, xmlNode *node, ProcessGroup *parent);
	//! Process Remote Process Group
	void parseRemoteProcessGroup(xmlDoc *doc, xmlNode *node, ProcessGroup *parent);

	//! Process Processor Node YAML
	void parseProcessorNodeYaml(YAML::Node processorNode, ProcessGroup *parent);
	//! Process Port YAML
	void parsePortYaml(YAML::Node *portNode, ProcessGroup *parent, TransferDirection direction);
	//! Process Root Processor Group YAML
	void parseRootProcessGroupYaml(YAML::Node rootNode);
	//! Process Property YAML
	void parseProcessorPropertyYaml(YAML::Node *doc, YAML::Node *node, Processor *processor);
	//! Process connection YAML
	void parseConnectionYaml(YAML::Node *node, ProcessGroup *parent);
	//! Process Remote Process Group YAML
	void parseRemoteProcessGroupYaml(YAML::Node *node, ProcessGroup *parent);
	//! Parse Properties Node YAML for a processor
	void parsePropertiesNodeYaml(YAML::Node *propertiesNode, Processor *processor);

	static FlowController *_flowController;

	//! Constructor
	/*!
	 * Create a new Flow Controller
	 */
	FlowController(std::string name = DEFAULT_ROOT_GROUP_NAME);

	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	FlowController(const FlowController &parent);
	FlowController &operator=(const FlowController &parent);

};

#endif
