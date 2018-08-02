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

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, utils::Identifier &uuid)
    : ProcessGroup(type, name, uuid, 0, 0) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, utils::Identifier &uuid, int version)
    : ProcessGroup(type, name, uuid, version, 0) {
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name, utils::Identifier &uuid, int version, ProcessGroup *parent)
    : logger_(logging::LoggerFactory<ProcessGroup>::getLogger()),
      name_(name),
      type_(type),
      config_version_(version),
      parent_process_group_(parent) {
  if (uuid == nullptr) {
    id_generator_->generate(uuid_);
  } else {
    uuid_ = uuid;
  }

  yield_period_msec_ = 0;
  transmitting_ = false;
  transport_protocol_ = "RAW";

  logger_->log_debug("ProcessGroup %s created", name_);
}

ProcessGroup::ProcessGroup(ProcessGroupType type, std::string name)
    : logger_(logging::LoggerFactory<ProcessGroup>::getLogger()),
      name_(name),
      type_(type),
      config_version_(0),
      parent_process_group_(0) {
  id_generator_->generate(uuid_);

  yield_period_msec_ = 0;
  transmitting_ = false;
  transport_protocol_ = "RAW";

  logger_->log_debug("ProcessGroup %s created", name_);
}

ProcessGroup::~ProcessGroup() {
  for (auto &&connection : connections_) {
    connection->drain();
  }

  for (std::set<ProcessGroup *>::iterator it = child_process_groups_.begin(); it != child_process_groups_.end(); ++it) {
    ProcessGroup *processGroup(*it);
    delete processGroup;
  }
}

bool ProcessGroup::isRootProcessGroup() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return (type_ == ROOT_PROCESS_GROUP);
}

void ProcessGroup::addProcessor(std::shared_ptr<Processor> processor) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (processors_.find(processor) == processors_.end()) {
    // We do not have the same processor in this process group yet
    processors_.insert(processor);
    logger_->log_debug("Add processor %s into process group %s", processor->getName(), name_);
  }
}

void ProcessGroup::removeProcessor(std::shared_ptr<Processor> processor) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (processors_.find(processor) != processors_.end()) {
    // We do have the same processor in this process group yet
    processors_.erase(processor);
    logger_->log_debug("Remove processor %s from process group %s", processor->getName(), name_);
  }
}

void ProcessGroup::addProcessGroup(ProcessGroup *child) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (child_process_groups_.find(child) == child_process_groups_.end()) {
    // We do not have the same child process group in this process group yet
    child_process_groups_.insert(child);
    logger_->log_debug("Add child process group %s into process group %s", child->getName(), name_);
  }
}

void ProcessGroup::removeProcessGroup(ProcessGroup *child) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (child_process_groups_.find(child) != child_process_groups_.end()) {
    // We do have the same child process group in this process group yet
    child_process_groups_.erase(child);
    logger_->log_debug("Remove child process group %s from process group %s", child->getName(), name_);
  }
}

void ProcessGroup::startProcessing(TimerDrivenSchedulingAgent *timeScheduler, EventDrivenSchedulingAgent *eventScheduler) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    // Start all the processor node, input and output ports
    for (auto processor : processors_) {
      logger_->log_debug("Starting %s", processor->getName());

      if (!processor->isRunning() && processor->getScheduledState() != DISABLED) {
        if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
          timeScheduler->schedule(processor);
        else if (processor->getSchedulingStrategy() == EVENT_DRIVEN)
          eventScheduler->schedule(processor);
      }
    }
    // Start processing the group
    for (auto processGroup : child_process_groups_) {
      processGroup->startProcessing(timeScheduler, eventScheduler);
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process group start processing");
    throw;
  }
}

void ProcessGroup::stopProcessing(TimerDrivenSchedulingAgent *timeScheduler, EventDrivenSchedulingAgent *eventScheduler) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    // Stop all the processor node, input and output ports
    for (std::set<std::shared_ptr<Processor> >::iterator it = processors_.begin(); it != processors_.end(); ++it) {
      std::shared_ptr<Processor> processor(*it);
      if (processor->getSchedulingStrategy() == TIMER_DRIVEN)
        timeScheduler->unschedule(processor);
      else if (processor->getSchedulingStrategy() == EVENT_DRIVEN)
        eventScheduler->unschedule(processor);
    }

    for (std::set<ProcessGroup *>::iterator it = child_process_groups_.begin(); it != child_process_groups_.end(); ++it) {
      ProcessGroup *processGroup(*it);
      processGroup->stopProcessing(timeScheduler, eventScheduler);
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process group stop processing");
    throw;
  }
}

std::shared_ptr<Processor> ProcessGroup::findProcessor(utils::Identifier &uuid) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::shared_ptr<Processor> ret = NULL;
  for (auto processor : processors_) {
    logger_->log_debug("find processor %s", processor->getName());
    utils::Identifier processorUUID;

    if (processor->getUUID(processorUUID)) {
      if (uuid == processorUUID) {
        return processor;
      }
    }
  }
  for (auto processGroup : child_process_groups_) {
    logger_->log_debug("find processor child %s", processGroup->getName());
    std::shared_ptr<Processor> processor = processGroup->findProcessor(uuid);
    if (processor)
      return processor;
  }
  return ret;
}

void ProcessGroup::addControllerService(const std::string &nodeId, std::shared_ptr<core::controller::ControllerServiceNode> &node) {
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
  std::shared_ptr<Processor> ret = NULL;

  for (auto processor : processors_) {
    logger_->log_debug("Current processor is %s", processor->getName());
    processor_vec.push_back(processor);
  }
  for (auto processGroup : child_process_groups_) {
    processGroup->getAllProcessors(processor_vec);
  }
}

std::shared_ptr<Processor> ProcessGroup::findProcessor(const std::string &processorName) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::shared_ptr<Processor> ret = NULL;
  for (auto processor : processors_) {
    logger_->log_debug("Current processor is %s", processor->getName());
    if (processor->getName() == processorName)
      return processor;
  }
  for (auto processGroup : child_process_groups_) {
    std::shared_ptr<Processor> processor = processGroup->findProcessor(processorName);
    if (processor)
      return processor;
  }
  return ret;
}

void ProcessGroup::updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto processor : processors_) {
    if (processor->getName() == processorName) {
      processor->setProperty(propertyName, propertyValue);
    }
  }
  for (auto processGroup : child_process_groups_) {
    processGroup->updatePropertyValue(processorName, propertyName, propertyValue);
  }
  return;
}

void ProcessGroup::getConnections(std::map<std::string, std::shared_ptr<Connection>> &connectionMap) {
  for (auto connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection;
    connectionMap[connection->getName()] = connection;
  }
  for (auto processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::getConnections(std::map<std::string, std::shared_ptr<Connectable>> &connectionMap) {
  for (auto connection : connections_) {
    connectionMap[connection->getUUIDStr()] = connection;
    connectionMap[connection->getName()] = connection;
  }
  for (auto processGroup : child_process_groups_) {
    processGroup->getConnections(connectionMap);
  }
}

void ProcessGroup::addConnection(std::shared_ptr<Connection> connection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connections_.find(connection) == connections_.end()) {
    // We do not have the same connection in this process group yet
    connections_.insert(connection);
    logger_->log_debug("Add connection %s into process group %s", connection->getName(), name_);
    utils::Identifier sourceUUID;
    std::shared_ptr<Processor> source = NULL;
    connection->getSourceUUID(sourceUUID);
    source = this->findProcessor(sourceUUID);
    if (source)
      source->addConnection(connection);
    std::shared_ptr<Processor> destination = NULL;
    utils::Identifier destinationUUID;
    connection->getDestinationUUID(destinationUUID);
    destination = this->findProcessor(destinationUUID);
    if (destination && destination != source)
      destination->addConnection(connection);
  }
}

void ProcessGroup::removeConnection(std::shared_ptr<Connection> connection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connections_.find(connection) != connections_.end()) {
    // We do not have the same connection in this process group yet
    connections_.erase(connection);
    logger_->log_debug("Remove connection %s into process group %s", connection->getName(), name_);
    utils::Identifier sourceUUID;
    std::shared_ptr<Processor> source = NULL;
    connection->getSourceUUID(sourceUUID);
    source = this->findProcessor(sourceUUID);
    if (source)
      source->removeConnection(connection);
    std::shared_ptr<Processor> destination = NULL;
    utils::Identifier destinationUUID;
    connection->getDestinationUUID(destinationUUID);
    destination = this->findProcessor(destinationUUID);
    if (destination && destination != source)
      destination->removeConnection(connection);
  }
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
