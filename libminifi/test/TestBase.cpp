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

#include "spdlog/spdlog.h"

void LogTestController::setLevel(const std::string name, spdlog::level::level_enum level) {
  logger_->log_info("Setting log level for %s to %s", name, spdlog::level::to_str(level));
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
    std::string state_dir_name_template = "/tmp/teststate.XXXXXX";
    std::vector<char> state_dir_buf(state_dir_name_template.c_str(),
                                   state_dir_name_template.c_str() + state_dir_name_template.size() + 1);
    if (mkdtemp(state_dir_buf.data()) == nullptr) {
      throw std::runtime_error("Failed to create temporary directory for state");
    }
    state_dir_ = state_dir_buf.data();
  } else {
    state_dir_ = state_dir;
  }
  state_manager_provider_ = core::ProcessContext::getOrCreateDefaultStateManagerProvider(controller_services_provider_, state_dir_.c_str());
}

TestPlan::~TestPlan() {
  controller_services_provider_->clearControllerServices();
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name, const std::initializer_list<core::Relationship>& relationships,
                                                        bool linkToPrevious) {
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
    termination_ = *(relationships.begin());
  } else {
    std::shared_ptr<core::Processor> last = processor_queue_.back();

    if (last == nullptr) {
      last = processor;
      termination_ = *(relationships.begin());
    }

    std::stringstream connection_name;
    connection_name << last->getUUIDStr() << "-to-" << processor->getUUIDStr();
    logger_->log_info("Creating %s connection for proc %d", connection_name.str(), processor_queue_.size() + 1);
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());

    for (const auto& relationship : relationships) {
      connection->addRelationship(relationship);
    }

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

  // std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider_, prov_repo_, flow_repo_, configuration_, content_repo_);

  auto contextBuilder = core::ClassLoader::getDefaultClassLoader().instantiate<core::ProcessContextBuilder>("ProcessContextBuilder");

  contextBuilder = contextBuilder->withContentRepository(content_repo_)->withFlowFileRepository(flow_repo_)->withProvider(controller_services_provider_)->withProvenanceRepository(prov_repo_)->withConfiguration(configuration_);

  auto context = contextBuilder->build(node);

  processor_contexts_.push_back(context);

  processor_queue_.push_back(processor);

  return processor;
}

std::shared_ptr<core::Processor> TestPlan::addProcessor(const std::string &processor_name, utils::Identifier& uuid, const std::string &name,
                                                        const std::initializer_list<core::Relationship>& relationships, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(processor_name, uuid);
  if (nullptr == ptr) {
    throw std::exception();
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

  utils::Identifier uuid;

  utils::IdGenerator::getIdGenerator()->generate(uuid);

  return addProcessor(processor_name, uuid, name, relationships, linkToPrevious);
}

std::shared_ptr<core::controller::ControllerServiceNode> TestPlan::addController(const std::string &controller_name, const std::string &name) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  utils::Identifier uuid;

  utils::IdGenerator::getIdGenerator()->generate(uuid);

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
  return location + 1 < processor_queue_.size();
}

bool TestPlan::runCurrentProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify) {
  if (!finalized) {
    finalize();
  }
  logger_->log_info("Rerunning current processor %d, processor_queue_.size %d, processor_contexts_.size %d", location, processor_queue_.size(), processor_contexts_.size());
  std::lock_guard<std::recursive_mutex> guard(mutex);

  std::shared_ptr<core::Processor> processor = processor_queue_.at(location);
  std::shared_ptr<core::ProcessContext> context = processor_contexts_.at(location);
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
  return location + 1 < processor_queue_.size();
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

  for (auto& controller_service_node : controller_service_nodes_) {
    controller_service_node->enable();
  }

  finalized = true;
}

