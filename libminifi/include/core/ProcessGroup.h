/**
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
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>
#include <utility>
#include <tuple>

#include "Processor.h"
#include "Exception.h"
#include "TimerDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "CronDrivenSchedulingAgent.h"
#include "Port.h"
#include "core/logging/Logger.h"
#include "controller/ControllerServiceNode.h"
#include "controller/ControllerServiceMap.h"
#include "utils/Id.h"
#include "utils/BaseHTTPClient.h"
#include "utils/CallBackTimer.h"
#include "range/v3/algorithm/find_if.hpp"

struct ProcessGroupTestAccessor;

namespace org::apache::nifi::minifi {

namespace state {
class StateController;
class ProcessorController;
}  // namespace state

namespace core {

enum ProcessGroupType {
  ROOT_PROCESS_GROUP = 0,
  SIMPLE_PROCESS_GROUP,
  REMOTE_PROCESS_GROUP,
};

#define ONSCHEDULE_RETRY_INTERVAL 30000  // millisecs

class ProcessGroup : public CoreComponent {
  friend struct ::ProcessGroupTestAccessor;
 public:
  enum class Traverse {
    ExcludeChildren,
    IncludeChildren
  };

  ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid, int version, ProcessGroup *parent);
  ProcessGroup(ProcessGroupType type, std::string name);
  ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid);
  ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid, int version);
  // Destructor
  ~ProcessGroup() override;
  // Set URL
  void setURL(std::string url) {
    url_ = std::move(url);
  }
  // Get URL
  std::string getURL() {
    return (url_);
  }
  // SetTransmitting
  void setTransmitting(bool val) {
    transmitting_ = val;
  }
  void setTimeout(uint64_t time) {
    timeout_ = time;
  }
  uint64_t getTimeout() {
    return timeout_;
  }
  // setInterface
  void setInterface(std::string &ifc) {
    local_network_interface_ = ifc;
  }
  std::string getInterface() {
    return local_network_interface_;
  }
  void setTransportProtocol(std::string &protocol) {
    transport_protocol_ = protocol;
  }
  std::string getTransportProtocol() {
    return transport_protocol_;
  }
  void setHttpProxyHost(std::string &host) {
    proxy_.host = host;
  }
  std::string getHttpProxyHost() const {
    return proxy_.host;
  }
  void setHttpProxyUserName(std::string &username) {
    proxy_.username = username;
  }
  void setHttpProxyPassWord(std::string &password) {
    proxy_.password = password;
  }
  void setHttpProxyPort(int port) {
    proxy_.port = port;
  }
  utils::HTTPProxy getHTTPProxy() {
    return proxy_;
  }
  // Set Processor yield period in MilliSecond
  void setYieldPeriodMsec(std::chrono::milliseconds period) {
    yield_period_msec_ = period;
  }
  // Get Processor yield period in MilliSecond
  std::chrono::milliseconds getYieldPeriodMsec() {
    return (yield_period_msec_);
  }

  void setOnScheduleRetryPeriod(int64_t period) {
    onschedule_retry_msec_ = period;
  }

  int64_t getOnScheduleRetryPeriod() {
    return onschedule_retry_msec_;
  }

  int getVersion() const {
    return config_version_;
  }

  void startProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler,
                       const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler,
                       const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler);

  void stopProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler,
                      const std::shared_ptr<EventDrivenSchedulingAgent>& eventScheduler,
                      const std::shared_ptr<CronDrivenSchedulingAgent>& cronScheduler,
                      const std::function<bool(const Processor*)>& filter = nullptr);

  bool isRemoteProcessGroup();
  // set parent process group
  void setParent(ProcessGroup *parent) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    parent_process_group_ = parent;
  }
  // get parent process group
  ProcessGroup *getParent() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return parent_process_group_;
  }
  [[maybe_unused]] std::tuple<Processor*, bool> addProcessor(std::unique_ptr<Processor> processor);
  void addPort(std::unique_ptr<Port> port);
  void addProcessGroup(std::unique_ptr<ProcessGroup> child);
  void addConnection(std::unique_ptr<Connection> connection);
  const std::set<Port*>& getPorts() const {
    return ports_;
  }

  Port* findPortById(const utils::Identifier& uuid) const;
  Port* findChildPortById(const utils::Identifier& uuid) const;

  template <typename Fun>
  Processor* findProcessor(Fun condition, Traverse traverse) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = ranges::find_if(processors_, condition);
    if (found != ranges::end(processors_)) {
      return found->get();
    }
    for (const auto& processGroup : child_process_groups_) {
      if (processGroup->isRemoteProcessGroup() || traverse == Traverse::IncludeChildren) {
        const auto processor = processGroup->findProcessor(condition, traverse);
        if (processor) {
          return processor;
        }
      }
    }
    return nullptr;
  }
  // findProcessor based on UUID
  Processor* findProcessorById(const utils::Identifier& uuid, Traverse traverse = Traverse::IncludeChildren) const;
  // findProcessor based on name
  Processor* findProcessorByName(const std::string &processorName, Traverse traverse = Traverse::IncludeChildren) const;

  void getAllProcessors(std::vector<Processor*>& processor_vec) const;

  /**
   * Add controller service
   * @param nodeId node identifier
   * @param node controller service node.
   */
  void addControllerService(const std::string &nodeId, const std::shared_ptr<core::controller::ControllerServiceNode> &node);

  /**
   * Find controllerservice node will search child groups until the nodeId is found.
   * @param node node identifier
   * @return controller service node, if it exists.
   */
  std::shared_ptr<core::controller::ControllerServiceNode> findControllerService(const std::string &nodeId);

  // update property value
  void updatePropertyValue(const std::string& processorName, const std::string& propertyName, const std::string& propertyValue);

  void getConnections(std::map<std::string, Connection*>& connectionMap);

  void getConnections(std::map<std::string, Connectable*>& connectionMap);

  void getFlowFileContainers(std::map<std::string, Connectable*>& containers) const;

  void drainConnections();

  std::size_t getTotalFlowFileCount() const;

  void verify() const;

 protected:
  void startProcessingProcessors(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler, const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler, const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler); // NOLINT

  // version
  int config_version_;
  // Process Group Type
  const ProcessGroupType type_;
  // Processors (ProcessNode) inside this process group which include Remote Process Group input/Output port
  std::set<std::unique_ptr<Processor>> processors_;
  std::set<Processor*> failed_processors_;
  std::set<Port*> ports_;
  std::set<std::unique_ptr<ProcessGroup>> child_process_groups_;
  // Connections between the processor inside the group;
  std::set<std::unique_ptr<Connection>> connections_;
  // Parent Process Group
  ProcessGroup* parent_process_group_;
  // Yield Period in Milliseconds
  std::atomic<std::chrono::milliseconds> yield_period_msec_;
  std::atomic<uint64_t> timeout_;
  std::atomic<int64_t> onschedule_retry_msec_;

  // URL
  std::string url_;
  // local network interface
  std::string local_network_interface_;
  // Transmitting
  std::atomic<bool> transmitting_;
  // http proxy
  utils::HTTPProxy proxy_;
  std::string transport_protocol_;

  // controller services

  core::controller::ControllerServiceMap controller_service_map_;

 private:
  static Port* findPortById(const std::set<Port*>& ports, const utils::Identifier& uuid);

  // Mutex for protection
  mutable std::recursive_mutex mutex_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProcessGroup(const ProcessGroup &parent);
  ProcessGroup &operator=(const ProcessGroup &parent);
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  std::unique_ptr<utils::CallBackTimer> onScheduleTimer_;
};
}  // namespace core
}  // namespace org::apache::nifi::minifi
