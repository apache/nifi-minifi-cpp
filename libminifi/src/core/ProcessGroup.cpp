/**
 * @file ProcessGroup.cpp
 * ProcessGroup class implementation
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
#include "core/ProcessGroup.h"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include "core/Processor.h"
#include "core/state/ProcessorController.h"
#include "core/state/UpdateController.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

std::shared_ptr<utils::IdGenerator> ProcessGroup::id_generator_ = utils::IdGenerator::getIdGenerator();

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid)
    : ProcessGroup(type, std::move(name), uuid, 0, nullptr) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid, int version)
    : ProcessGroup(type, std::move(name), uuid, version, nullptr) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, const utils::Identifier& uuid, int version, ProcessGroup* parent)
    : CoreComponent(std::move(name), uuid, id_generator_),
      config_version_(version),
      type_(type),
      parent_process_group_(parent),
      logger_(logging::LoggerFactory<ProcessGroup>::getLogger()) {
  yield_period_msec_ = 0ms;

  if (parent_process_group_ != nullptr) {
    onschedule_retry_msec_ = parent_process_group_->getOnScheduleRetryPeriod();
  } else {
    onschedule_retry_msec_ = ONSCHEDULE_RETRY_INTERVAL;
  }
  transmitting_ = false;
  transport_protocol_ = "RAW";

  logger_->log_debug("ProcessGroup %s created", name_);
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name)
    : CoreComponent(std::move(name), {}, id_generator_),
      config_version_(0),
      type_(type),
      parent_process_group_(nullptr),
      logger_(logging::LoggerFactory<ProcessGroup>::getLogger()) {
  yield_period_msec_ = 0ms;
  onschedule_retry_msec_ = ONSCHEDULE_RETRY_INTERVAL;
  transmitting_ = false;
  transport_protocol_ = "RAW";

  logger_->log_debug("ProcessGroup %s created", name_);
}

ProcessGroup::~ProcessGroup() {
  if (onScheduleTimer_) {
    onScheduleTimer_->stop();
  }

  for (auto&& connection : connections_) {
    connection->drain(false);
  }
}

bool ProcessGroup::isRemoteProcessGroup() {
  return (type_ == REMOTE_PROCESS_GROUP);
}


std::tuple<Processor*, bool> ProcessGroup::addProcessor(std::unique_ptr<Processor> processor) {
  gsl_Expects(processor);
  const auto name = processor->getName();
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const auto [iter, inserted] = processors_.insert(std::move(processor));
  if (inserted) {
    logger_->log_debug("Add processor %s into process group %s", name, name_);
  } else {
    logger_->log_debug("Not adding processor %s into process group %s, as it is already there", name, name_);
  }
  return std::make_tuple(iter->get(), inserted);
}

void ProcessGroup::addPort(std::unique_ptr<Port> port) {
  auto [processor, inserted] = addProcessor(std::move(port));
  if (inserted) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ports_.insert(static_cast<Port*>(processor));
  }
}

void ProcessGroup::addProcessGroup(std::unique_ptr<ProcessGroup> child) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (child_process_groups_.find(child) == child_process_groups_.end()) {
    // We do not have the same child process group in this process group yet
    logger_->log_debug("Add child process group %s into process group %s", child->getName(), name_);
    child_process_groups_.emplace(std::move(child));
  }
}

void ProcessGroup::startProcessingProcessors(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler,
    const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler, const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler) {
  std::unique_lock<std::recursive_mutex> lock(mutex_);

  std::set<Processor*> failed_processors;

  for (const auto processor : failed_processors_) {
    try {
      logger_->log_debug("Starting %s", processor->getName());
      switch (processor->getSchedulingStrategy()) {
        case TIMER_DRIVEN:
          timeScheduler->schedule(processor);
          break;
        case EVENT_DRIVEN:
          eventScheduler->schedule(processor);
          break;
        case CRON_DRIVEN:
          cronScheduler->schedule(processor);
          break;
      }
    }
    catch (const std::exception &e) {
      logger_->log_error("Failed to start processor %s (%s): %s", processor->getUUIDStr(), processor->getName(), e.what());
      failed_processors.insert(processor);
    }
    catch (...) {
      logger_->log_error("Failed to start processor %s (%s)", processor->getUUIDStr(), processor->getName());
      failed_processors.insert(processor);
    }
  }
  failed_processors_ = std::move(failed_processors);

  for (const auto processor : failed_processors_) {
    try {
      processor->onUnSchedule();
    } catch (const std::exception& ex) {
      logger_->log_error("Exception occured during unscheduling processor: %s (%s), type: %s, what: %s", processor->getUUIDStr(), processor->getName(), typeid(ex).name(), ex.what());
    } catch (...) {
      logger_->log_error("Exception occured during unscheduling processor: %s (%s), type: %s", processor->getUUIDStr(), processor->getName(), getCurrentExceptionTypeName());
    }
  }

  if (!onScheduleTimer_ && !failed_processors_.empty() && onschedule_retry_msec_ > 0) {
    logger_->log_info("Retrying failed processors in %lld msec", onschedule_retry_msec_.load());
    auto func = [this, eventScheduler, cronScheduler, timeScheduler]() {
      this->startProcessingProcessors(timeScheduler, eventScheduler, cronScheduler);
    };
    onScheduleTimer_ = std::make_unique<utils::CallBackTimer>(std::chrono::milliseconds(onschedule_retry_msec_), func);
    onScheduleTimer_->start();
  } else if (failed_processors_.empty() && onScheduleTimer_) {
    onScheduleTimer_->stop();
  }
}

void ProcessGroup::startProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler, const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler,
                                   const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    // All processors are marked as failed.
    for (auto& processor : processors_) {
      failed_processors_.insert(processor.get());
    }

    // Start all the processor node, input and output ports
    startProcessingProcessors(timeScheduler, eventScheduler, cronScheduler);

    // Start processing the group
    for (auto& processGroup : child_process_groups_) {
      processGroup->startProcessing(timeScheduler, eventScheduler, cronScheduler);
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process group start processing, type: %s", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessGroup::stopProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler, const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler,
                                  const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler, const std::function<bool(const Processor*)>& filter) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (onScheduleTimer_) {
    onScheduleTimer_->stop();
  }

  onScheduleTimer_.reset();

  try {
    // Stop all the processor node, input and output ports
    for (const auto &processor : processors_) {
      if (filter && !filter(processor.get())) {
        continue;
      }
      logger_->log_debug("Stopping %s", processor->getName());
      switch (processor->getSchedulingStrategy()) {
        case TIMER_DRIVEN:
          timeScheduler->unschedule(processor.get());
          break;
        case EVENT_DRIVEN:
          eventScheduler->unschedule(processor.get());
          break;
        case CRON_DRIVEN:
          cronScheduler->unschedule(processor.get());
          break;
      }
    }

    for (auto& childGroup : child_process_groups_) {
      childGroup->stopProcessing(timeScheduler, eventScheduler, cronScheduler, filter);
    }
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception type: %s, what: %s", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process group stop processing, type: %s", getCurrentExceptionTypeName());
    throw;
  }
}

Processor* ProcessGroup::findProcessorById(const utils::Identifier& uuid, Traverse traverse) const {
  const auto id_matches = [&] (const std::unique_ptr<Processor>& processor) {
    logger_->log_trace("Searching for processor by id, checking processor %s", processor->getName());
    utils::Identifier processorUUID = processor->getUUID();
    return processorUUID && uuid == processorUUID;
  };
  return findProcessor(id_matches, traverse);
}

Processor* ProcessGroup::findProcessorByName(const std::string &processorName, Traverse traverse) const {
  const auto name_matches = [&] (const std::unique_ptr<Processor>& processor) {
    logger_->log_trace("Searching for processor by name, checking processor %s", processor->getName());
    return processor->getName() == processorName;
  };
  return findProcessor(name_matches, traverse);
}

void ProcessGroup::addControllerService(const std::string &nodeId, const std::shared_ptr<core::controller::ControllerServiceNode> &node) {
  controller_service_map_.put(nodeId, node);
}

/**
 * Find controllerservice node will search child groups until the nodeId is found.
 * @param node node identifier
 * @return controller service node, if it exists.
 */
std::shared_ptr<core::controller::ControllerServiceNode> ProcessGroup::findControllerService(const std::string &nodeId) {
  return controller_service_map_.getControllerServiceNode(nodeId);
}

void ProcessGroup::getAllProcessors(std::vector<Processor*>& processor_vec) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  for (const auto& processor : processors_) {
    logger_->log_trace("Collecting all processors, current processor is %s", processor->getName());
    processor_vec.push_back(processor.get());
  }
  for (const auto& processGroup : child_process_groups_) {
    processGroup->getAllProcessors(processor_vec);
  }
}

void ProcessGroup::updatePropertyValue(const std::string& processorName, const std::string& propertyName, const std::string& propertyValue) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto& processor : processors_) {
    if (processor->getName() == processorName) {
      processor->setProperty(propertyName, propertyValue);
    }
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->updatePropertyValue(processorName, propertyName, propertyValue);
  }
}

void ProcessGroup::getConnections(std::map<std::string, Connection*>& connectionMap) {
  for (auto& connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection.get();
    connectionMap[connection->getName()] = connection.get();
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::getConnections(std::map<std::string, Connectable*>& connectionMap) {
  for (auto& connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection.get();
    connectionMap[connection->getName()] = connection.get();
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::getFlowFileContainers(std::map<std::string, Connectable*>& containers) const {
  for (auto& connection : connections_) {
    containers[connection->getUUIDStr()] = connection.get();
    containers[connection->getName()] = connection.get();
  }
  for (auto& processor : processors_) {
    // processors can also own FlowFiles
    containers[processor->getUUIDStr()] = processor.get();
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getFlowFileContainers(containers);
  }
}

Port* ProcessGroup::findPortById(const std::set<Port*>& ports, const utils::Identifier& uuid) {
  const auto found = ranges::find_if(ports, [&](auto port) {
      utils::Identifier port_uuid = port->getUUID();
      return port_uuid && uuid == port_uuid;
    });
  if (found != ranges::cend(ports)) {
    return *found;
  }
  return nullptr;
}

Port* ProcessGroup::findPortById(const utils::Identifier& uuid) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return findPortById(ports_, uuid);
}

Port* ProcessGroup::findChildPortById(const utils::Identifier& uuid) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (const auto& processGroup : child_process_groups_) {
    const auto& ports = processGroup->getPorts();
    if (auto port = findPortById(ports, uuid)) {
      return port;
    }
  }
  return nullptr;
}

void ProcessGroup::addConnection(std::unique_ptr<Connection> connection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  auto [insertPos, inserted] = connections_.insert(std::move(connection));
  if (!inserted) {
    return;
  }

  auto& insertedConnection = *insertPos;

  logger_->log_debug("Add connection %s into process group %s", insertedConnection->getName(), name_);
  // only allow connections between processors of the same process group or in/output ports of child process groups
  // check input and output ports connection restrictions inside and outside a process group
  Processor* source = findPortById(insertedConnection->getSourceUUID());
  if (source && static_cast<Port*>(source)->getPortType() == PortType::OUTPUT) {
    logger_->log_error("Output port [id = '%s'] cannot be a source inside the process group in the connection [name = '%s', id = '%s']",
                       insertedConnection->getSourceUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
    source = nullptr;
  } else if (!source) {
    source = findChildPortById(insertedConnection->getSourceUUID());
    if (source && static_cast<Port*>(source)->getPortType() == PortType::INPUT) {
      logger_->log_error("Input port [id = '%s'] cannot be a source outside the process group in the connection [name = '%s', id = '%s']",
                          insertedConnection->getSourceUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
      source = nullptr;
    } else if (!source) {
      source = findProcessorById(insertedConnection->getSourceUUID(), Traverse::ExcludeChildren);
      if (!source) {
        logger_->log_error("Cannot find the source processor with id '%s' for the connection [name = '%s', id = '%s']",
                          insertedConnection->getSourceUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
      }
    }
  }

  if (source) {
    source->addConnection(insertedConnection.get());
  }

  Processor* destination = findPortById(insertedConnection->getDestinationUUID());
  if (destination && static_cast<Port*>(destination)->getPortType() == PortType::INPUT) {
    logger_->log_error("Input port [id = '%s'] cannot be a destination inside the process group in the connection [name = '%s', id = '%s']",
                       insertedConnection->getDestinationUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
    destination = nullptr;
  } else if (!destination) {
    destination = findChildPortById(insertedConnection->getDestinationUUID());
    if (destination && static_cast<Port*>(destination)->getPortType() == PortType::OUTPUT) {
      logger_->log_error("Output port [id = '%s'] cannot be a destination outside the process group in the connection [name = '%s', id = '%s']",
                          insertedConnection->getDestinationUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
      destination = nullptr;
    } else if (!destination) {
      destination = findProcessorById(insertedConnection->getDestinationUUID(), Traverse::ExcludeChildren);
      if (!destination) {
        logger_->log_error("Cannot find the destination processor with id '%s' for the connection [name = '%s', id = '%s']",
                          insertedConnection->getDestinationUUID().to_string(), insertedConnection->getName(), insertedConnection->getUUIDStr());
      }
    }
  }

  if (destination && destination != source) {
    destination->addConnection(insertedConnection.get());
  }
}

void ProcessGroup::drainConnections() {
  for (auto&& connection : connections_) {
    connection->drain(false);
  }

  for (auto& childGroup : child_process_groups_) {
    childGroup->drainConnections();
  }
}

std::size_t ProcessGroup::getTotalFlowFileCount() const {
  std::size_t sum = 0;
  for (const auto& conn : connections_) {
    sum += gsl::narrow<std::size_t>(conn->getQueueSize());
  }

  for (const auto& childGroup : child_process_groups_) {
    sum += childGroup->getTotalFlowFileCount();
  }
  return sum;
}

void ProcessGroup::verify() const {
  for (const auto& processor : processors_) {
    processor->validateAnnotations();
  }
}

}  // namespace org::apache::nifi::minifi::core
