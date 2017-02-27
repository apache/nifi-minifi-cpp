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
#include "yaml-cpp/yaml.h"

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
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"
#include "RemoteProcessorGroupPort.h"
#include "Provenance.h"
#include "GetFile.h"
#include "PutFile.h"
#include "TailFile.h"
#include "ListenSyslog.h"
#include "ListenHTTP.h"
#include "ExecuteProcess.h"
#include "AppendHostInfo.h"
// OpenSSL related
#include <openssl/ssl.h>
#include <openssl/err.h>

//! Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""
#define DEFAULT_FLOW_YAML_FILE_NAME "conf/flow.yml"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"

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

/**
 * Flow Controller class. Generally used by FlowController factory
 * as a singleton.
 */
class FlowController {
 public:
  static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
  static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;

  //! Destructor
  virtual ~FlowController() {
  }
  //! Set FlowController Name
  virtual void setName(std::string name) {
    _name = name;
  }
  //! Get Flow Controller Name
  virtual std::string getName(void) {
    return (_name);
  }
  //! Set UUID
  virtual void setUUID(uuid_t uuid) {
    uuid_copy(_uuid, uuid);
  }
  //! Get UUID
  virtual bool getUUID(uuid_t uuid) {
    if (uuid) {
      uuid_copy(uuid, _uuid);
      return true;
    } else
      return false;
  }
  //! Set MAX TimerDrivenThreads
  virtual void setMaxTimerDrivenThreads(int number) {
    _maxTimerDrivenThreads = number;
  }
  //! Get MAX TimerDrivenThreads
  virtual int getMaxTimerDrivenThreads() {
    return _maxTimerDrivenThreads;
  }
  //! Set MAX EventDrivenThreads
  virtual void setMaxEventDrivenThreads(int number) {
    _maxEventDrivenThreads = number;
  }
  //! Get MAX EventDrivenThreads
  virtual int getMaxEventDrivenThreads() {
    return _maxEventDrivenThreads;
  }
  //! Get the provenance repository
  virtual ProvenanceRepository *getProvenanceRepository() {
    return this->_provenanceRepo;
  }
  //! Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  virtual void load() = 0;

  //! Whether the Flow Controller is start running
  virtual bool isRunning() {
    return _running.load();
  }
  //! Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return _initialized.load();
  }
  //! Start to run the Flow Controller which internally start the root process group and all its children
  virtual bool start() = 0;
  //! Unload the current flow YAML, clean the root process group and all its children
  virtual void stop(bool force) = 0;
  //! Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const uint64_t timeToWaitMs) = 0;
  //! Unload the current flow xml, clean the root process group and all its children
  virtual void unload() = 0;
  //! Load new xml
  virtual void reload(std::string yamlFile) = 0;
  //! update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
    if (_root)
      _root->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  //! Create Processor (Node/Input/Output Port) based on the name
  virtual Processor *createProcessor(std::string name, uuid_t uuid) = 0;
  //! Create Root Processor Group
  virtual ProcessGroup *createRootProcessGroup(std::string name,
                                               uuid_t uuid) = 0;
  //! Create Remote Processor Group
  virtual ProcessGroup *createRemoteProcessGroup(std::string name,
                                                 uuid_t uuid) = 0;
  //! Create Connection
  virtual Connection *createConnection(std::string name, uuid_t uuid) = 0;
  //! set 8 bytes SerialNumber
  virtual void setSerialNumber(uint8_t *number) {
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
  //! Whether it is running
  std::atomic<bool> _running;
  //! Whether it has already been initialized (load the flow XML already)
  std::atomic<bool> _initialized;
  //! Provenance Repo
  ProvenanceRepository *_provenanceRepo;
  //! Flow Engines
  //! Flow Timer Scheduler
  TimerDrivenSchedulingAgent _timerScheduler;
  //! Flow Event Scheduler
  EventDrivenSchedulingAgent _eventScheduler;
  //! Controller Service
  //! Config
  //! Site to Site Server Listener
  //! Heart Beat
  //! FlowControl Protocol
  FlowControlProtocol *_protocol;

  FlowController()
      : _root(0),
        _maxTimerDrivenThreads(0),
        _maxEventDrivenThreads(0),
        _running(false),
        _initialized(false),
        _provenanceRepo(0),
        _protocol(0),
        logger_(Logger::getLogger()) {
  }

 private:

  //! Logger
  std::shared_ptr<Logger> logger_;

};

/**
 * Flow Controller implementation that defines the typical flow.
 * of events.
 */
class FlowControllerImpl : public FlowController {
 public:

  //! Destructor
  virtual ~FlowControllerImpl();

  //! Life Cycle related function
  //! Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  void load();
  //! Start to run the Flow Controller which internally start the root process group and all its children
  bool start();
  //! Stop to run the Flow Controller which internally stop the root process group and all its children
  void stop(bool force);
  //! Asynchronous function trigger unloading and wait for a period of time
  void waitUnload(const uint64_t timeToWaitMs);
  //! Unload the current flow xml, clean the root process group and all its children
  void unload();
  //! Load new xml
  void reload(std::string yamlFile);
  //! update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
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

  //! Constructor
  /*!
   * Create a new Flow Controller
   */
  FlowControllerImpl(std::string name = DEFAULT_ROOT_GROUP_NAME);

  friend class FlowControlFactory;

 private:

  //! Mutex for protection
  std::mutex _mtx;
  //! Logger
  std::shared_ptr<Logger> logger_;
  Configure *configure_;
  //! Process Processor Node YAML
  void parseProcessorNodeYaml(YAML::Node processorNode, ProcessGroup *parent);
  //! Process Port YAML
  void parsePortYaml(YAML::Node *portNode, ProcessGroup *parent,
                     TransferDirection direction);
  //! Process Root Processor Group YAML
  void parseRootProcessGroupYaml(YAML::Node rootNode);
  //! Process Property YAML
  void parseProcessorPropertyYaml(YAML::Node *doc, YAML::Node *node,
                                  Processor *processor);
  //! Process connection YAML
  void parseConnectionYaml(YAML::Node *node, ProcessGroup *parent);
  //! Process Remote Process Group YAML
  void parseRemoteProcessGroupYaml(YAML::Node *node, ProcessGroup *parent);
  //! Parse Properties Node YAML for a processor
  void parsePropertiesNodeYaml(YAML::Node *propertiesNode,
                               Processor *processor);

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  FlowControllerImpl(const FlowController &parent);
  FlowControllerImpl &operator=(const FlowController &parent);

};

/**
 * Flow Controller factory that creates flow controllers or gets the
 * assigned instance.
 */
class FlowControllerFactory {
 public:
  //! Get the singleton flow controller
  static FlowController * getFlowController(FlowController *instance = 0) {
    if (!_flowController) {
      if (NULL == instance)
        _flowController = createFlowController();
      else
        _flowController = instance;
    }
    return _flowController;
  }

  //! Get the singleton flow controller
  static FlowController * createFlowController() {
    return dynamic_cast<FlowController*>(new FlowControllerImpl());
  }
 private:
  static FlowController *_flowController;
};

#endif
