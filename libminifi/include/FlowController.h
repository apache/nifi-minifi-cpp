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
<<<<<<< HEAD
#include "Configure.h"
=======
#include "yaml-cpp/yaml.h"

#include "properties/Configure.h"
>>>>>>> MINIFI-217: Update based on PR and rebasing against master
#include "core/Relationship.h"
#include "FlowFileRecord.h"
#include "Connection.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/RealTimeDataCollector.h"
#include "TimerDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"
#include "RemoteProcessorGroupPort.h"
#include "provenance/Provenance.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/TailFile.h"
#include "processors/ListenSyslog.h"
#include "processors/ListenHTTP.h"
#include "processors/ExecuteProcess.h"
#include "processors/AppendHostInfo.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""
#define DEFAULT_FLOW_YAML_FILE_NAME "conf/flow.yml"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"

struct ProcessorConfig {
  std::string id;
  std::string name;
  std::string javaClass;
  std::string maxConcurrentTasks;
  std::string schedulingStrategy;
  std::string schedulingPeriod;
  std::string penalizationPeriod;
  std::string yieldPeriod;
  std::string runDurationNanos;
  std::vector<std::string> autoTerminatedRelationships;
  std::vector<core::Property> properties;
};

/**
 * Flow Controller class. Generally used by FlowController factory
 * as a singleton.
 */
class FlowController : public core::CoreComponent {
 public:
  static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
  static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;

  // Destructor
  virtual ~FlowController() {
  }

  // Set MAX TimerDrivenThreads
  virtual void setMaxTimerDrivenThreads(int number) {
    max_timer_driven_threads_ = number;
  }
  // Get MAX TimerDrivenThreads
  virtual int getMaxTimerDrivenThreads() {
    return max_timer_driven_threads_;
  }
  // Set MAX EventDrivenThreads
  virtual void setMaxEventDrivenThreads(int number) {
    max_event_driven_threads_ = number;
  }
  // Get MAX EventDrivenThreads
  virtual int getMaxEventDrivenThreads() {
    return max_event_driven_threads_;
  }
  // Get the provenance repository
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() {
    return this->provenance_repo_;
  }

  // Get the flowfile repository
  virtual std::shared_ptr<core::Repository> getFlowFileRepository() {
    return this->flow_file_repo_;
  }

  // Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  virtual void load() = 0;

  // Whether the Flow Controller is start running
  virtual bool isRunning() {
    return running_.load();
  }
  // Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start to run the Flow Controller which internally start the root process group and all its children
  virtual bool start() = 0;
  // Unload the current flow YAML, clean the root process group and all its children
  virtual void stop(bool force) = 0;
  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const uint64_t timeToWaitMs) = 0;
  // Unload the current flow xml, clean the root process group and all its children
  virtual void unload() = 0;
  // Load new xml
  virtual void reload(std::string yamlFile) = 0;
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
    if (root_)
      root_->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  // Create Processor (Node/Input/Output Port) based on the name
  virtual std::shared_ptr<core::Processor> createProcessor(std::string name,
                                                           uuid_t uuid) = 0;
  // Create Root Processor Group
  virtual core::ProcessGroup *createRootProcessGroup(std::string name,
                                                     uuid_t uuid) = 0;
  // Create Remote Processor Group
  virtual core::ProcessGroup *createRemoteProcessGroup(std::string name,
                                                       uuid_t uuid) = 0;
  // Create Connection
  virtual std::shared_ptr<Connection> createConnection(std::string name,
                                                       uuid_t uuid) = 0;
  // set 8 bytes SerialNumber
  virtual void setSerialNumber(uint8_t *number) {
    protocol_->setSerialNumber(number);
  }

 protected:

  // Configuration File Name
  std::string configuration_file_name_;
  // NiFi property File Name
  std::string properties_file_name_;
  // Root Process Group
  core::ProcessGroup *root_;
  // MAX Timer Driven Threads
  int max_timer_driven_threads_;
  // MAX Event Driven Threads
  int max_event_driven_threads_;
  // Config
  // FlowFile Repo
  // Whether it is running
  std::atomic<bool> running_;
  // conifiguration filename
  std::string configuration_filename_;
  // Whether it has already been initialized (load the flow XML already)
  std::atomic<bool> initialized_;
  // Provenance Repo
  std::shared_ptr<core::Repository> provenance_repo_;

  // FlowFile Repo
  std::shared_ptr<core::Repository> flow_file_repo_;

  // Flow Engines
  // Flow Timer Scheduler
  TimerDrivenSchedulingAgent _timerScheduler;
  // Flow Event Scheduler
  EventDrivenSchedulingAgent _eventScheduler;
  // Controller Service
  // Config
  // Site to Site Server Listener
  // Heart Beat
  // FlowControl Protocol
  FlowControlProtocol *protocol_;

  FlowController(std::shared_ptr<core::Repository> provenance_repo,
                 std::shared_ptr<core::Repository> flow_file_repo)
      : CoreComponent(core::getClassName<FlowController>()), root_(0),
        max_timer_driven_threads_(0),
        max_event_driven_threads_(0),
        running_(false),
        initialized_(false),
        provenance_repo_(provenance_repo),
        flow_file_repo_(flow_file_repo),
        protocol_(0),
        _timerScheduler(provenance_repo_),
        _eventScheduler(provenance_repo_){
    if (provenance_repo == nullptr)
      throw std::runtime_error("Provenance Repo should not be null");
    if (flow_file_repo == nullptr)
          throw std::runtime_error("Flow Repo should not be null");
  }


};

class FlowControllerImpl : public FlowController {
 public:

  // Destructor
  virtual ~FlowControllerImpl();

  // Life Cycle related function
  // Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  void load();
  // Start to run the Flow Controller which internally start the root process group and all its children
  bool start();
  // Stop to run the Flow Controller which internally stop the root process group and all its children
  void stop(bool force);
  // Asynchronous function trigger unloading and wait for a period of time
  void waitUnload(const uint64_t timeToWaitMs);
  // Unload the current flow xml, clean the root process group and all its children
  void unload();
  // Load Flow File from persistent Flow Repo
  void loadFlowRepo();
  // Load new xml
  void reload(std::string yamlFile);
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
    if (root_)
      root_->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  // Create Processor (Node/Input/Output Port) based on the name
  std::shared_ptr<core::Processor> createProcessor(std::string name,
                                                   uuid_t uuid);
  // Create Root Processor Group
  core::ProcessGroup *createRootProcessGroup(std::string name, uuid_t uuid);
  // Create Remote Processor Group
  core::ProcessGroup *createRemoteProcessGroup(std::string name, uuid_t uuid);
  // Create Connection
  std::shared_ptr<Connection> createConnection(std::string name, uuid_t uuid);

  // Constructor
  /*!
   * Create a new Flow Controller
   */
  FlowControllerImpl(std::shared_ptr<core::Repository> repo,std::shared_ptr<core::Repository> flow_file_repo,
                     std::string name = DEFAULT_ROOT_GROUP_NAME);

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  FlowControllerImpl(const FlowController &parent) = delete;
  FlowControllerImpl &operator=(const FlowController &parent) = delete;

 private:

  std::mutex mutex_;

  // configuration object
  Configure *configure_;
  // Process Processor Node YAML
  void parseProcessorNodeYaml(YAML::Node processorNode,
                              core::ProcessGroup *parent);
  // Process Port YAML
  void parsePortYaml(YAML::Node *portNode, core::ProcessGroup *parent,
                     TransferDirection direction);
  // Process Root Processor Group YAML
  void parseRootProcessGroupYaml(YAML::Node rootNode);
  // Process Property YAML
  void parseProcessorPropertyYaml(YAML::Node *doc, YAML::Node *node,
                                  std::shared_ptr<core::Processor> processor);
  // Process connection YAML
  void parseConnectionYaml(YAML::Node *node, core::ProcessGroup *parent);
  // Process Remote Process Group YAML
  void parseRemoteProcessGroupYaml(YAML::Node *node,
                                   core::ProcessGroup *parent);
  // Parse Properties Node YAML for a processor
  void parsePropertiesNodeYaml(YAML::Node *propertiesNode,
                               std::shared_ptr<core::Processor> processor);

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
