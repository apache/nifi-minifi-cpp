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
#include "properties/Configure.h"
#include "core/Relationship.h"
#include "FlowFileRecord.h"
#include "Connection.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"
#include "core/FlowConfiguration.h"
#include "TimerDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"

#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""

/**
 * Flow Controller class. Generally used by FlowController factory
 * as a singleton.
 */
class FlowController : public core::CoreComponent {
 public:
  static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
  static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;

  /**
   * Flow controller constructor
   */
  FlowController(std::shared_ptr<core::Repository> provenance_repo,
                 std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure,
                 std::unique_ptr<core::FlowConfiguration> flow_configuration,
                 const std::string name = DEFAULT_ROOT_GROUP_NAME,
                 bool headless_mode = false);

  // Destructor
  virtual ~FlowController();

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
  virtual void load();

  // Whether the Flow Controller is start running
  virtual bool isRunning() {
    return running_.load();
  }
  // Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start to run the Flow Controller which internally start the root process group and all its children
  virtual bool start();
  // Unload the current flow YAML, clean the root process group and all its children
  virtual void stop(bool force);
  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const uint64_t timeToWaitMs);
  // Unload the current flow xml, clean the root process group and all its children
  virtual void unload();
  // Load new xml
  virtual void reload(std::string yamlFile);
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
    if (root_ != nullptr)
      root_->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  // set 8 bytes SerialNumber
  virtual void setSerialNumber(uint8_t *number) {
    protocol_->setSerialNumber(number);
  }

 protected:

  // function to load the flow file repo.
  void loadFlowRepo();

  /**
   * Initializes flow controller paths.
   */
  virtual void initializePaths(const std::string &adjustedFilename);

  // flow controller mutex
  std::recursive_mutex mutex_;

  // Configuration File Name
  std::string configuration_file_name_;
  // NiFi property File Name
  std::string properties_file_name_;
  // Root Process Group
  std::unique_ptr<core::ProcessGroup> root_;
  // MAX Timer Driven Threads
  int max_timer_driven_threads_;
  // MAX Event Driven Threads
  int max_event_driven_threads_;
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

  // flow configuration object.
  std::unique_ptr<core::FlowConfiguration> flow_configuration_;

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
