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

#include "./TestBase.h"

TestPlan::TestPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo,
                   const std::shared_ptr<minifi::state::response::FlowVersion> &flow_version, const std::shared_ptr<minifi::Configure> &configuration)
    : configuration_(configuration),
      content_repo_(content_repo),
      flow_repo_(flow_repo),
      prov_repo_(prov_repo),
      finalized(false),
      location(-1),
      current_flowfile_(nullptr),
      flow_version_(flow_version),
      logger_(logging::LoggerFactory<TestPlan>::getLogger()) {
  stream_factory = org::apache::nifi::minifi::io::StreamFactory::getInstance(std::make_shared<minifi::Configure>());
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name, core::Relationship relationship, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  utils::Identifier uuid;

  utils::IdGenerator::getIdGenerator()->generate(uuid);

  processor->setStreamFactory(stream_factory);
  // initialize the processor
  processor->initialize();
  processor->setFlowIdentifier(flow_version_->getFlowIdentifier());

  processor_mapping_[processor->getUUIDStr()] = processor;

  if (!linkToPrevious) {
    termination_ = relationship;
  } else {
    std::shared_ptr<core::Processor> last = processor_queue_.back();

    if (last == nullptr) {
      last = processor;
      termination_ = relationship;
    }

    std::stringstream connection_name;
    connection_name << last->getUUIDStr() << "-to-" << processor->getUUIDStr();
    logger_->log_info("Creating %s connection for proc %d", connection_name.str(), processor_queue_.size() + 1);
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());
    connection->addRelationship(relationship);

    // link the connections so that we can test results at the end for this
    connection->setSource(last);
    connection->setDestination(processor);

    utils::Identifier uuid_copy, uuid_copy_next;
    last->getUUID(uuid_copy);
    connection->setSourceUUID(uuid_copy);
    processor->getUUID(uuid_copy_next);
    connection->setDestinationUUID(uuid_copy_next);
    last->addConnection(connection);
    if (last != processor) {
      processor->addConnection(connection);
    }
    relationships_.push_back(connection);
  }

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);

  processor_nodes_.push_back(node);

  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);
  processor_contexts_.push_back(context);

  processor_queue_.push_back(processor);

  return processor;
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::string &processor_name, const std::string &name, core::Relationship relationship, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  utils::Identifier uuid;

  utils::IdGenerator::getIdGenerator()->generate(uuid);

  std::cout << "generated " << uuid.to_string() << std::endl;

  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(processor_name, uuid);
  if (nullptr == ptr) {
    throw std::exception();
  }
  std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);

  processor->setName(name);

  return addProcessor(processor, name, relationship, linkToPrevious);
}

bool TestPlan::setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value, bool dynamic) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  int32_t i = 0;
  logger_->log_info("Attempting to set property %s %s for %s", prop, value, proc->getName());
  for (i = 0; i < processor_queue_.size(); i++) {
    if (processor_queue_.at(i) == proc) {
      break;
    }
  }

  if (i >= processor_queue_.size() || i < 0 || i >= processor_contexts_.size()) {
    return false;
  }

  if (dynamic) {
    return processor_contexts_.at(i)->setDynamicProperty(prop, value);
  } else {
    return processor_contexts_.at(i)->setProperty(prop, value);
  }
}

void TestPlan::reset() {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  process_sessions_.clear();
  factories_.clear();
  location = -1;
  for (auto proc : processor_queue_) {
    while (proc->getActiveTasks() > 0) {
      proc->decrementActiveTask();
    }
  }
}

bool TestPlan::runNextProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify) {
  if (!finalized) {
    finalize();
  }
  logger_->log_info("Running next processor %d, processor_queue_.size %d, processor_contexts_.size %d", location, processor_queue_.size(), processor_contexts_.size());
  std::lock_guard<std::recursive_mutex> guard(mutex);
  location++;
  std::shared_ptr<core::Processor> processor = processor_queue_.at(location);
  std::shared_ptr<core::ProcessContext> context = processor_contexts_.at(location);
  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);
  factories_.push_back(factory);
  if (std::find(configured_processors_.begin(), configured_processors_.end(), processor) == configured_processors_.end()) {
    processor->onSchedule(context, factory);
    configured_processors_.push_back(processor);
  }
  std::shared_ptr<core::ProcessSession> current_session = std::make_shared<core::ProcessSession>(context);
  process_sessions_.push_back(current_session);
  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  if (verify != nullptr) {
    verify(context, current_session);
  } else {
    logger_->log_info("Running %s", processor->getName());
    processor->onTrigger(context, current_session);
  }
  current_session->commit();
  current_flowfile_ = current_session->get();
  return location + 1 < processor_queue_.size();
}

std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> TestPlan::getProvenanceRecords() {
  return process_sessions_.at(location)->getProvenanceReporter()->getEvents();
}

std::shared_ptr<core::FlowFile> TestPlan::getCurrentFlowFile() {
  return current_flowfile_;
}

std::shared_ptr<minifi::Connection> TestPlan::buildFinalConnection(std::shared_ptr<core::Processor> processor, bool setDest) {
  std::stringstream connection_name;
  std::shared_ptr<core::Processor> last = processor;
  connection_name << last->getUUIDStr() << "-to-" << processor->getUUIDStr();
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());
  connection->addRelationship(termination_);

  // link the connections so that we can test results at the end for this
  connection->setSource(last);
  if (setDest)
    connection->setDestination(processor);

  utils::Identifier uuid_copy;
  last->getUUID(uuid_copy);
  connection->setSourceUUID(uuid_copy);
  if (setDest)
    connection->setDestinationUUID(uuid_copy);

  processor->addConnection(connection);
  return connection;
}

void TestPlan::finalize() {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  if (relationships_.size() > 0) {
    relationships_.push_back(buildFinalConnection(processor_queue_.back()));
  } else {
    for (auto processor : processor_queue_) {
      relationships_.push_back(buildFinalConnection(processor, true));
    }
  }

  finalized = true;
}

