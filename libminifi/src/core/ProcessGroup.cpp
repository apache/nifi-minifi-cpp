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
#include <time.h>
#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> ProcessGroup::id_generator_ = utils::IdGenerator::getIdGenerator();

ProcessGroup::ProcessGroup(ProcessGroupType type, const std::string& name, const utils::Identifier& uuid)
    : ProcessGroup(type, name, uuid, 0, 0) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, const std::string& name, const utils::Identifier& uuid, int version)
    : ProcessGroup(type, name, uuid, version, 0) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, const std::string& name, const utils::Identifier& uuid, int version, ProcessGroup* parent)
    : CoreComponent(name, uuid, id_generator_),
      config_version_(version),
      type_(type),
      parent_process_group_(parent),
      logger_(logging::LoggerFactory<ProcessGroup>::getLogger()) {
  yield_period_msec_ = 0;

  if (parent_process_group_ != 0) {
    onschedule_retry_msec_ = parent_process_group_->getOnScheduleRetryPeriod();
  } else {
    onschedule_retry_msec_ = ONSCHEDULE_RETRY_INTERVAL;
  }
  transmitting_ = false;
  transport_protocol_ = "RAW";

  logger_->log_debug("ProcessGroup %s created", name_);
}

ProcessGroup::ProcessGroup(ProcessGroupType type, const std::string& name)
    : CoreComponent(name, {}, id_generator_),
      config_version_(0),
      type_(type),
      parent_process_group_(0),
      logger_(logging::LoggerFactory<ProcessGroup>::getLogger()) {
  yield_period_msec_ = 0;
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

bool ProcessGroup::isRootProcessGroup() {
  return (type_ == ROOT_PROCESS_GROUP);
}

bool ProcessGroup::isRemoteProcessGroup() {
  return (type_ == REMOTE_PROCESS_GROUP);
}


void ProcessGroup::addProcessor(const std::shared_ptr<Processor>& processor) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (processors_.find(processor) == processors_.end()) {
    // We do not have the same processor in this process group yet
    processors_.insert(processor);
    logger_->log_debug("Add processor %s into process group %s", processor->getName(), name_);
  }
}

void ProcessGroup::removeProcessor(const std::shared_ptr<Processor>& processor) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (processors_.find(processor) != processors_.end()) {
    // We do have the same processor in this process group yet
    processors_.erase(processor);
    logger_->log_debug("Remove processor %s from process group %s", processor->getName(), name_);
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

  std::set<std::shared_ptr<Processor> > failed_processors;

  for (const auto &processor : failed_processors_) {
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

  for (auto& processor : failed_processors_) {
    try {
      processor->onUnSchedule();
    } catch (...) {
      logger_->log_error("Exception occured during unscheduling processor: %s (%s)", processor->getUUIDStr(), processor->getName());
    }
  }

  if (!onScheduleTimer_ && !failed_processors_.empty() && onschedule_retry_msec_ > 0) {
    logger_->log_info("Retrying failed processors in %lld msec", onschedule_retry_msec_.load());
    auto func = [this, eventScheduler, cronScheduler, timeScheduler]() {
      this->startProcessingProcessors(timeScheduler, eventScheduler, cronScheduler);
    };
    onScheduleTimer_.reset(new utils::CallBackTimer(std::chrono::milliseconds(onschedule_retry_msec_), func));
    onScheduleTimer_->start();
  } else if (failed_processors_.empty() && onScheduleTimer_) {
    onScheduleTimer_->stop();
  }
}

void ProcessGroup::startProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler, const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler,
                                   const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    failed_processors_ = processors_;  // All processors are marked as failed.

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
    logger_->log_debug("Caught Exception during process group start processing");
    throw;
  }
}

void ProcessGroup::stopProcessing(const std::shared_ptr<TimerDrivenSchedulingAgent>& timeScheduler, const std::shared_ptr<EventDrivenSchedulingAgent> &eventScheduler,
                                  const std::shared_ptr<CronDrivenSchedulingAgent> &cronScheduler, const std::function<bool(const std::shared_ptr<Processor>&)>& filter) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (onScheduleTimer_) {
    onScheduleTimer_->stop();
  }

  onScheduleTimer_.reset();

  try {
    // Stop all the processor node, input and output ports
    for (const auto &processor : processors_) {
      if (!filter(processor)) {
        continue;
      }
      logger_->log_debug("Stopping %s", processor->getName());
      switch (processor->getSchedulingStrategy()) {
        case TIMER_DRIVEN:
          timeScheduler->unschedule(processor);
          break;
        case EVENT_DRIVEN:
          eventScheduler->unschedule(processor);
          break;
        case CRON_DRIVEN:
          cronScheduler->unschedule(processor);
          break;
      }
    }

    for (auto& childGroup : child_process_groups_) {
      childGroup->stopProcessing(timeScheduler, eventScheduler, cronScheduler, filter);
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process group stop processing");
    throw;
  }
}

std::shared_ptr<Processor> ProcessGroup::findProcessorById(const utils::Identifier& uuid, Traverse traverse) const {
  const auto id_matches = [&] (const std::shared_ptr<Processor>& processor) {
    logger_->log_trace("Searching for processor by id, checking processor %s", processor->getName());
    utils::Identifier processorUUID = processor->getUUID();
    return processorUUID && uuid == processorUUID;
  };
  return findProcessor(id_matches, traverse);
}

std::shared_ptr<Processor> ProcessGroup::findProcessorByName(const std::string &processorName, Traverse traverse) const {
  const auto name_matches = [&] (const std::shared_ptr<Processor>& processor) {
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

void ProcessGroup::getAllProcessors(std::vector<std::shared_ptr<Processor>> &processor_vec) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  for (auto& processor : processors_) {
    logger_->log_trace("Collecting all processors, current processor is %s", processor->getName());
    processor_vec.push_back(processor);
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getAllProcessors(processor_vec);
  }
}

void ProcessGroup::updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue) {
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

void ProcessGroup::getConnections(std::map<std::string, std::shared_ptr<Connection>> &connectionMap) {
  for (auto& connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection;
    connectionMap[connection->getName()] = connection;
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::getConnections(std::map<std::string, std::shared_ptr<Connectable>> &connectionMap) {
  for (auto& connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection;
    connectionMap[connection->getName()] = connection;
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::getFlowFileContainers(std::map<std::string, std::shared_ptr<Connectable>> &containers) const {
  for (auto& connection : connections_) {
    containers[connection->getUUIDStr()] = connection;
    containers[connection->getName()] = connection;
  }
  for (auto& processor : processors_) {
    // processors can also own FlowFiles
    containers[processor->getUUIDStr()] = processor;
  }
  for (auto& processGroup : child_process_groups_) {
    processGroup->getFlowFileContainers(containers);
  }
}

void ProcessGroup::addConnection(const std::shared_ptr<Connection>& connection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connections_.find(connection) == connections_.end()) {
    // We do not have the same connection in this process group yet
    connections_.insert(connection);
    logger_->log_debug("Add connection %s into process group %s", connection->getName(), name_);
    // only allow connections between processors of the same process group
    std::shared_ptr<Processor> source = this->findProcessorById(connection->getSourceUUID(), Traverse::ExcludeChildren);
    if (source) {
      source->addConnection(connection);
    } else {
      logger_->log_error("Cannot find the source processor with id '%s' for the connection [name = '%s', id = '%s']",
                         connection->getSourceUUID().to_string(), connection->getName(), connection->getUUIDStr());
    }
    std::shared_ptr<Processor> destination = this->findProcessorById(connection->getDestinationUUID(), Traverse::ExcludeChildren);
    if (!destination) {
      logger_->log_error("Cannot find the destination processor with id '%s' for the connection [name = '%s', id = '%s']",
                         connection->getDestinationUUID().to_string(), connection->getName(), connection->getUUIDStr());
    }
    if (destination && destination != source) {
      destination->addConnection(connection);
    }
  }
}

void ProcessGroup::removeConnection(const std::shared_ptr<Connection>& connection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connections_.find(connection) != connections_.end()) {
    // We do not have the same connection in this process group yet
    connections_.erase(connection);
    logger_->log_debug("Remove connection %s into process group %s", connection->getName(), name_);
    std::shared_ptr<Processor> source = this->findProcessorById(connection->getSourceUUID());
    if (source)
      source->removeConnection(connection);
    std::shared_ptr<Processor> destination = this->findProcessorById(connection->getDestinationUUID());
    if (destination && destination != source)
      destination->removeConnection(connection);
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

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
