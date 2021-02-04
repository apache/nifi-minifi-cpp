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
#include "utils/IntegrationTestUtils.h"

#include "spdlog/spdlog.h"

void LogTestController::setLevel(const std::string name, spdlog::level::level_enum level) {
  const auto levelView(spdlog::level::to_string_view(level));
  logger_->log_info("Setting log level for %s to %s", name, std::string(levelView.begin(), levelView.end()));
  std::string adjusted_name = name;
  const std::string clazz = "class ";
  auto haz_clazz = name.find(clazz);
  if (haz_clazz == 0)
    adjusted_name = name.substr(clazz.length(), name.length() - clazz.length());
  if (config && config->shortenClassNames()) {
    utils::ClassUtils::shortenClassName(adjusted_name, adjusted_name);
  }
  spdlog::get(adjusted_name)->set_level(level);
}

TestPlan::TestPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo,
                   const std::shared_ptr<minifi::state::response::FlowVersion> &flow_version, const std::shared_ptr<minifi::Configure> &configuration, const char* state_dir)
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
  controller_services_ = std::make_shared<core::controller::ControllerServiceMap>();
  controller_services_provider_ = std::make_shared<core::controller::StandardControllerServiceProvider>(controller_services_, nullptr, configuration_);
  /* Inject the default state provider ahead of ProcessContext to make sure we have a unique state directory */
  if (state_dir == nullptr) {
    state_dir_.reset(new StateDir());
  } else {
    state_dir_.reset(new StateDir(state_dir));
  }
  state_manager_provider_ = core::ProcessContext::getOrCreateDefaultStateManagerProvider(controller_services_provider_.get(), configuration_, state_dir_->getPath().c_str());
}

TestPlan::~TestPlan() {
  for (auto& processor : configured_processors_) {
    processor->setScheduledState(core::ScheduledState::STOPPED);
  }
  for (auto& connection : relationships_) {
    // This is a patch solving circular references between processors and connections
    connection->setSource(nullptr);
    connection->setDestination(nullptr);
  }
  controller_services_provider_->clearControllerServices();
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string& /*name*/, const std::initializer_list<core::Relationship>& relationships,
    bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);
  processor->setStreamFactory(stream_factory);
  // initialize the processor
  processor->initialize();
  processor->setFlowIdentifier(flow_version_->getFlowIdentifier());
  processor_mapping_[processor->getUUID()] = processor;
  if (!linkToPrevious) {
    termination_ = *(relationships.begin());
  } else {
    std::shared_ptr<core::Processor> last = processor_queue_.back();
    if (last == nullptr) {
      last = processor;
      termination_ = *(relationships.begin());
    }
    std::stringstream connection_name;
    connection_name << last->getUUIDStr() << "-to-" << processor->getUUIDStr();
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());
    logger_->log_info("Creating %s connection for proc %d", connection_name.str(), processor_queue_.size() + 1);

    for (const auto& relationship : relationships) {
      connection->addRelationship(relationship);
    }
    // link the connections so that we can test results at the end for this
    connection->setSource(last);
    connection->setDestination(processor);

    connection->setSourceUUID(last->getUUID());
    connection->setDestinationUUID(processor->getUUID());
    last->addConnection(connection);
    if (last != processor) {
      processor->addConnection(connection);
    }
    relationships_.push_back(connection);
  }
  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  processor_nodes_.push_back(node);
  // std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);
  auto contextBuilder = core::ClassLoader::getDefaultClassLoader().instantiate<core::ProcessContextBuilder>("ProcessContextBuilder");
  contextBuilder = contextBuilder->withContentRepository(content_repo_)->withFlowFileRepository(flow_repo_)->withProvider(controller_services_provider_.get())->withProvenanceRepository(prov_repo_)->withConfiguration(configuration_);
  auto context = contextBuilder->build(node);
  processor_contexts_.push_back(context);
  processor_queue_.push_back(processor);
  return processor;
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::string &processor_name, const utils::Identifier& uuid, const std::string &name,
    const std::initializer_list<core::Relationship>& relationships, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(processor_name, uuid);
  if (nullptr == ptr) {
    throw std::runtime_error{fmt::format("Failed to instantiate processor name: {0} uuid: {1}", processor_name, uuid.to_string().c_str())};
  }
  std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);

  processor->setName(name);

  return addProcessor(processor, name, relationships, linkToPrevious);
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::string &processor_name, const std::string &name, const std::initializer_list<core::Relationship>& relationships,
    bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);
  return addProcessor(processor_name, utils::IdGenerator::getIdGenerator()->generate(), name, relationships, linkToPrevious);
}

std::shared_ptr<minifi::Connection> TestPlan::addConnection(const std::shared_ptr<core::Processor>& source_proc, const core::Relationship& source_relationship, const std::shared_ptr<core::Processor>& destination_proc) {
  std::stringstream connection_name;
  connection_name
    << (source_proc ? source_proc->getUUIDStr().c_str() : "none")
    << "-to-"
    << (destination_proc ? destination_proc->getUUIDStr().c_str() : "none");
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());

  connection->addRelationship(source_relationship);

  // link the connections so that we can test results at the end for this

  if (source_proc) {
    connection->setSource(source_proc);
    connection->setSourceUUID(source_proc->getUUID());
  }
  if (destination_proc) {
    connection->setDestination(destination_proc);
    connection->setDestinationUUID(destination_proc->getUUID());
  }
  if (source_proc) {
    source_proc->addConnection(connection);
  }
  if (source_proc != destination_proc && destination_proc) {
    destination_proc->addConnection(connection);
  }
  relationships_.push_back(connection);
  return connection;
}

std::shared_ptr<core::controller::ControllerServiceNode> TestPlan::addController(const std::string &controller_name, const std::string &name) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  utils::Identifier uuid = utils::IdGenerator::getIdGenerator()->generate();

  std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node =
      controller_services_provider_->createControllerService(controller_name, controller_name, name, true /*firstTimeAdded*/);
  if (controller_service_node == nullptr) {
    return nullptr;
  }

  controller_service_nodes_.push_back(controller_service_node);

  controller_service_node->initialize();
  controller_service_node->setUUID(uuid);
  controller_service_node->setName(name);

  return controller_service_node;
}

bool TestPlan::setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value, bool dynamic) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  size_t i = 0;
  logger_->log_info("Attempting to set property %s %s for %s", prop, value, proc->getName());
  for (i = 0; i < processor_queue_.size(); i++) {
    if (processor_queue_.at(i) == proc) {
      break;
    }
  }

  if (i >= processor_queue_.size() || i >= processor_contexts_.size()) {
    return false;
  }

  if (dynamic) {
    return processor_contexts_.at(i)->setDynamicProperty(prop, value);
  } else {
    return processor_contexts_.at(i)->setProperty(prop, value);
  }
}

bool TestPlan::setProperty(const std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node, const std::string &prop, const std::string &value, bool dynamic /*= false*/) {
  if (dynamic) {
    controller_service_node->setDynamicProperty(prop, value);
    return controller_service_node->getControllerServiceImplementation()->setDynamicProperty(prop, value);
  } else {
    controller_service_node->setProperty(prop, value);
    return controller_service_node->getControllerServiceImplementation()->setProperty(prop, value);
  }
}

void TestPlan::reset(bool reschedule) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  process_sessions_.clear();
  factories_.clear();
  location = -1;
  if (reschedule)
    configured_processors_.clear();
  for (auto proc : processor_queue_) {
    while (proc->getActiveTasks() > 0) {
      proc->decrementActiveTask();
    }
  }
}

std::vector<std::shared_ptr<core::Processor>>::iterator TestPlan::getProcessorItByUuid(const std::string& uuid) {
  const auto processor_node_matches_processor = [&uuid] (const std::shared_ptr<core::Processor>& processor) {
    return processor->getUUIDStr() == uuid;
  };
  auto processor_found_at = std::find_if(processor_queue_.begin(), processor_queue_.end(), processor_node_matches_processor);
  if (processor_found_at == processor_queue_.end()) {
    throw std::runtime_error("Processor not found in test plan.");
  }
  return processor_found_at;
}

std::shared_ptr<core::ProcessContext> TestPlan::getProcessContextForProcessor(const std::shared_ptr<core::Processor>& processor) {
  const auto contextMatchesProcessor = [&processor] (const std::shared_ptr<core::ProcessContext>& context) {
    return context->getProcessorNode()->getUUIDStr() ==  processor->getUUIDStr();
  };
  const auto context_found_at = std::find_if(processor_contexts_.begin(), processor_contexts_.end(), contextMatchesProcessor);
  if (context_found_at == processor_contexts_.end()) {
    throw std::runtime_error("Context not found in test plan.");
  }
  return *context_found_at;
}

void TestPlan::scheduleProcessor(const std::shared_ptr<core::Processor>& processor, const std::shared_ptr<core::ProcessContext>& context) {
  if (std::find(configured_processors_.begin(), configured_processors_.end(), processor) == configured_processors_.end()) {
    // Ordering on factories and list of configured processors do not matter
    std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);
    factories_.push_back(factory);
    processor->onSchedule(context, factory);
    configured_processors_.push_back(processor);
  }
}

void TestPlan::scheduleProcessor(const std::shared_ptr<core::Processor>& processor) {
  scheduleProcessor(processor, getProcessContextForProcessor(processor));
}

void TestPlan::scheduleProcessors() {
  for(std::size_t target_location = 0; target_location < processor_queue_.size(); ++target_location) {
    std::shared_ptr<core::Processor> processor = processor_queue_.at(target_location);
    std::shared_ptr<core::ProcessContext> context = processor_contexts_.at(target_location);
    scheduleProcessor(processor, context);
  }
}

bool TestPlan::runProcessor(const std::shared_ptr<core::Processor>& processor, const PreTriggerVerifier& verify) {
  const std::size_t processor_location = std::distance(processor_queue_.begin(), getProcessorItByUuid(processor->getUUIDStr()));
  return runProcessor(gsl::narrow<int>(processor_location), verify);
}

bool TestPlan::runProcessor(size_t target_location, const PreTriggerVerifier& verify) {
  std::lock_guard<std::recursive_mutex> guard(mutex);

  std::shared_ptr<core::Processor> processor = processor_queue_.at(target_location);
  std::shared_ptr<core::ProcessContext> context = processor_contexts_.at(target_location);
  scheduleProcessor(processor, context);
  std::shared_ptr<core::ProcessSession> current_session = std::make_shared<core::ProcessSession>(context);
  process_sessions_.push_back(current_session);
  current_flowfile_ = nullptr;
  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  if (verify != nullptr) {
    verify(context, current_session);
  } else {
    logger_->log_info("Running %s", processor->getName());
    processor->onTrigger(context, current_session);
  }
  current_session->commit();
  return gsl::narrow<size_t>(target_location + 1) < processor_queue_.size();
}

bool TestPlan::runNextProcessor(const PreTriggerVerifier& verify) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  ++location;
  return runProcessor(location, verify);
}

bool TestPlan::runCurrentProcessor(const PreTriggerVerifier& /*verify*/) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  return runProcessor(location);
}

bool TestPlan::runCurrentProcessorUntilFlowfileIsProduced(const std::chrono::seconds& wait_duration) {
  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  const auto isFlowFileProduced = [&] {
    runCurrentProcessor();
    const std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor_queue_.at(location));
    return std::any_of(connections.cbegin(), connections.cend(), [] (const minifi::Connection* conn) { return !conn->isEmpty(); });
  };
  return verifyEventHappenedInPollTime(wait_duration, isFlowFileProduced);
}

std::size_t TestPlan::getNumFlowFileProducedByCurrentProcessor() {
  const std::shared_ptr<core::Processor>& processor = processor_queue_.at(location);
  std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor);
  std::size_t num_flow_files = 0;
  for (auto connection : connections) {
    num_flow_files += connection->getQueueSize();
  }
  return num_flow_files;
}

std::shared_ptr<core::FlowFile> TestPlan::getFlowFileProducedByCurrentProcessor() {
  const std::shared_ptr<core::Processor>& processor = processor_queue_.at(location);
  std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor);
  for (auto connection : connections) {
    std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
    std::shared_ptr<core::FlowFile> flowfile = connection->poll(expiredFlowRecords);
    if (flowfile) {
      return flowfile;
    }
    if (expiredFlowRecords.empty()) {
      continue;
    }
    if (expiredFlowRecords.size() != 1) {
      throw std::runtime_error("Multiple expired flowfiles present in a single connection.");
    }
    return *expiredFlowRecords.begin();
  }
  return nullptr;
}

std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> TestPlan::getProvenanceRecords() {
  return process_sessions_.at(location)->getProvenanceReporter()->getEvents();
}

std::shared_ptr<core::FlowFile> TestPlan::getCurrentFlowFile() {
  if (current_flowfile_ == nullptr) {
    current_flowfile_ = process_sessions_.at(location)->get();
  }
  return current_flowfile_;
}

std::vector<minifi::Connection*> TestPlan::getProcessorOutboundConnections(const std::shared_ptr<core::Processor>& processor) {
  const auto is_processor_outbound_connection = [&processor] (const std::shared_ptr<minifi::Connection>& connection) {
    // A connection is outbound from a processor if its source uuid matches the processor
    return connection->getSource()->getUUIDStr() == processor->getUUIDStr();
  };
  std::vector<minifi::Connection*> connections;
  for (auto relationship : relationships_) {
    if (is_processor_outbound_connection(relationship)) {
      connections.emplace_back(relationship.get());
    }
  }
  return connections;
}


std::shared_ptr<core::ProcessContext> TestPlan::getCurrentContext() {
  return processor_contexts_.at(location);
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

  utils::Identifier uuid_copy = last->getUUID();
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

  for (auto& controller_service_node : controller_service_nodes_) {
    controller_service_node->enable();
  }

  finalized = true;
}

