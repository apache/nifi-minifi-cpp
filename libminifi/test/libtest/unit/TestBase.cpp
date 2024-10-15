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

#include "TestBase.h"

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessorNode.h"
#include "core/ProcessContextBuilder.h"
#include "core/PropertyDefinition.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/state/nodes/FlowInformation.h"
#include "core/controller/StandardControllerServiceProvider.h"
#include "ProvenanceTestHelper.h"
#include "utils/ClassUtils.h"
#include "TestUtils.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/span.h"
#include "LogUtils.h"

#include "fmt/format.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/dist_sink.h"

std::shared_ptr<LogTestController> LogTestController::getInstance(const std::shared_ptr<logging::LoggerProperties>& logger_properties) {
  static std::map<std::shared_ptr<logging::LoggerProperties>, std::shared_ptr<LogTestController>> map;
  auto fnd = map.find(logger_properties);
  if (fnd != std::end(map)) {
    return fnd->second;
  } else {
    // in practice I'd use a derivation here or another paradigm entirely but for the purposes of this test code
    // having extra overhead is negligible. this is the most readable and least impactful way
    map.insert(std::make_pair(logger_properties, std::shared_ptr<LogTestController>(new LogTestController(logger_properties))));
    return map.find(logger_properties)->second;
  }
}

void LogTestController::setLevel(std::string_view name, spdlog::level::level_enum level) {
  logger_->log_info("Setting log level for {} to {}", name, spdlog::level::to_string_view(level));
  std::string adjusted_name{name};
  const std::string clazz = "class ";
  auto haz_clazz = name.find(clazz);
  if (haz_clazz == 0)
    adjusted_name = name.substr(clazz.length(), name.length() - clazz.length());
  if (config && config->shortenClassNames()) {
    minifi::utils::ClassUtils::shortenClassName(adjusted_name, adjusted_name);
  }
  if (const auto spd_logger = logging::LoggerConfiguration::getSpdlogLogger(adjusted_name)) {
    spd_logger->set_level(level);
  }
}

std::shared_ptr<logging::Logger> LogTestController::getLoggerByClassName(std::string_view class_name, const std::optional<minifi::utils::Identifier>& id) {
  return config ? config->getLogger(class_name, id) : logging::LoggerConfiguration::getConfiguration().getLogger(class_name, id);
}

void LogTestController::setLevelByClassName(spdlog::level::level_enum level, std::string_view class_name) {
  if (config)
    config->getLogger(class_name);
  else
    logging::LoggerConfiguration::getConfiguration().getLogger(class_name);
  modified_loggers.emplace_back(class_name);
  setLevel(class_name, level);
  // also support shortened classnames
  if (config && config->shortenClassNames()) {
    std::string adjusted{class_name};
    if (minifi::utils::ClassUtils::shortenClassName(class_name, adjusted)) {
      modified_loggers.emplace_back(class_name);
      setLevel(class_name, level);
    }
  }
}

bool LogTestController::contains(const std::ostringstream& stream, const std::string& ending, std::chrono::milliseconds timeout, std::chrono::milliseconds sleep_interval) {
  return contains([&stream](){ return stream.str(); }, ending, timeout, sleep_interval);
}

bool LogTestController::contains(const std::string& ending, std::chrono::milliseconds timeout, std::chrono::milliseconds sleep_interval) const {
  return contains([this](){ return getLogs(); }, ending, timeout, sleep_interval);
}

bool LogTestController::contains(const std::function<std::string()>& log_string_getter, const std::string& ending, std::chrono::milliseconds timeout, std::chrono::milliseconds sleep_interval) {
  if (ending.length() == 0) {
    return false;
  }
  auto start = std::chrono::steady_clock::now();
  bool found = false;
  bool timed_out = false;
  do {
    std::string str = log_string_getter();
    found = (str.find(ending) != std::string::npos);
    auto now = std::chrono::steady_clock::now();
    timed_out = (now - start > timeout);
    if (!found && !timed_out) {
      std::this_thread::sleep_for(sleep_interval);
    }
  } while (!found && !timed_out);

  return found;
}

std::optional<std::smatch> LogTestController::matchesRegex(const std::string& regex_str, std::chrono::milliseconds timeout, std::chrono::milliseconds sleep_interval) const {
  if (regex_str.length() == 0) {
    return std::nullopt;
  }
  auto start = std::chrono::steady_clock::now();
  bool found = false;
  bool timed_out = false;
  std::regex matcher_regex(regex_str);
  std::smatch match;
  do {
    std::string str = getLogs();
    found = std::regex_search(str, match, matcher_regex);
    auto now = std::chrono::steady_clock::now();
    timed_out = (now - start > timeout);
    if (!found && !timed_out) {
      std::this_thread::sleep_for(sleep_interval);
    }
  } while (!found && !timed_out);

  logger_->log_info("{} {} in log output.", found ? "Successfully matched regex" : "Failed to match regex", regex_str);
  return found ? std::make_optional<std::smatch>(match) : std::nullopt;
}

int LogTestController::countOccurrences(const std::string& pattern) const {
  return minifi::utils::string::countOccurrences(getLogs(), pattern).second;
}

void LogTestController::reset() {
  for (auto const & name : modified_loggers) {
    setLevel(name, spdlog::level::err);
  }
  modified_loggers.clear();
  if (config)
    config = logging::LoggerConfiguration::newInstance();
  my_properties_ = std::make_shared<logging::LoggerProperties>();
  clear();
  init(nullptr);
}

void LogTestController::clear() {
  std::lock_guard<std::mutex> guard(*log_output_mutex_);
  gsl_Expects(log_output_ptr_);
  resetStream(*log_output_ptr_);
}

void LogTestController::resetStream(std::ostringstream& stream) {
  stream.str("");
  stream.clear();
}

void LogTestController::init(const std::shared_ptr<logging::LoggerProperties>& logger_props) {
  gsl_Expects(log_output_ptr_);
  my_properties_ = logger_props;
  bool initMain = false;
  if (nullptr == my_properties_) {
    my_properties_ = std::make_shared<logging::LoggerProperties>();
    initMain = true;
  }
  my_properties_->set("logger.root", "ERROR,ostream");
  my_properties_->set("logger." + std::string(minifi::core::className<LogTestController>()), "INFO");
  my_properties_->set("logger." + std::string(minifi::core::className<logging::LoggerConfiguration>()), "INFO");
  std::shared_ptr<spdlog::sinks::dist_sink_mt> dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
  dist_sink->add_sink(std::make_shared<StringStreamSink>(log_output_ptr_, log_output_mutex_, true));
  dist_sink->add_sink(std::make_shared<spdlog::sinks::stderr_sink_mt>());
  my_properties_->add_sink("ostream", dist_sink);
  if (initMain) {
    logging::LoggerConfiguration::getConfiguration().initialize(my_properties_);
    logger_ = logging::LoggerConfiguration::getConfiguration().getLogger(minifi::core::className<LogTestController>());
  } else {
    config = logging::LoggerConfiguration::newInstance();
    // create for test purposes. most tests use the main logging factory, but this exists to test the logging
    // framework itself.
    config->initialize(my_properties_);
    logger_ = config->getLogger(minifi::core::className<LogTestController>());
  }
}

LogTestController::LogTestController(const std::shared_ptr<logging::LoggerProperties>& loggerProps) {
  init(loggerProps);
}

TestPlan::TestPlan(std::shared_ptr<minifi::core::ContentRepository> content_repo, std::shared_ptr<minifi::core::Repository> flow_repo, std::shared_ptr<minifi::core::Repository> prov_repo,
                   std::shared_ptr<minifi::state::response::FlowVersion> flow_version, std::shared_ptr<minifi::Configure> configuration, const char* state_dir)
    : configuration_(std::move(configuration)),
      content_repo_(std::move(content_repo)),
      flow_repo_(std::move(flow_repo)),
      prov_repo_(std::move(prov_repo)),
      controller_services_provider_(std::make_shared<minifi::core::controller::StandardControllerServiceProvider>(
          std::make_unique<minifi::core::controller::ControllerServiceNodeMap>(), configuration_)),
      finalized(false),
      location(-1),
      current_flowfile_(nullptr),
      flow_version_(std::move(flow_version)),
      logger_(logging::LoggerFactory<TestPlan>::getLogger()) {
  /* Inject the default state storage ahead of ProcessContext to make sure we have a unique state directory */
  if (state_dir == nullptr) {
    state_dir_ = std::make_unique<TempDirectory>();
  } else {
    state_dir_ = std::make_unique<TempDirectory>(state_dir);
  }
  if (!configuration_->get(minifi::Configure::nifi_state_storage_local_path)) {
    configuration_->set(minifi::Configure::nifi_state_storage_local_path, state_dir_->getPath().string());
  }
  state_storage_ = minifi::core::ProcessContextImpl::getOrCreateDefaultStateStorage(controller_services_provider_.get(), configuration_);
}

TestPlan::~TestPlan() {
  for (auto& processor : configured_processors_) {
    processor->setScheduledState(minifi::core::ScheduledState::STOPPED);
  }
  for (auto& connection : relationships_) {
    // This is a patch solving circular references between processors and connections
    connection->setSource(nullptr);
    connection->setDestination(nullptr);
  }
  controller_services_provider_->clearControllerServices();
}

std::shared_ptr<minifi::core::Processor> TestPlan::addProcessor(const std::shared_ptr<minifi::core::Processor> &processor, const std::string& /*name*/,
    const std::initializer_list<minifi::core::Relationship>& relationships, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);
  // initialize the processor
  processor->initialize();
  processor->setFlowIdentifier(flow_version_->getFlowIdentifier());
  processor_mapping_[processor->getUUID()] = processor;
  if (!linkToPrevious) {
    if (!std::empty(relationships)) {
      termination_ = *(relationships.begin());
    }
  } else {
    std::shared_ptr<minifi::core::Processor> last = processor_queue_.back();
    if (last == nullptr) {
      last = processor;
      if (!std::empty(relationships)) {
        termination_ = *(relationships.begin());
      }
    }
    std::stringstream connection_name;
    connection_name << last->getUUIDStr() << "-to-" << processor->getUUIDStr();
    auto connection = std::make_unique<minifi::ConnectionImpl>(flow_repo_, content_repo_, connection_name.str());
    logger_->log_info("Creating {} connection for proc {}", connection_name.str(), processor_queue_.size() + 1);

    for (const auto& relationship : relationships) {
      connection->addRelationship(relationship);
    }
    // link the connections so that we can test results at the end for this
    connection->setSource(last.get());
    connection->setDestination(processor.get());

    connection->setSourceUUID(last->getUUID());
    connection->setDestinationUUID(processor->getUUID());
    last->addConnection(connection.get());
    if (last != processor) {
      processor->addConnection(connection.get());
    }
    relationships_.push_back(std::move(connection));
  }
  std::shared_ptr<minifi::core::ProcessorNode> node = std::make_shared<minifi::core::ProcessorNodeImpl>(processor.get());
  processor_nodes_.push_back(node);
  std::shared_ptr<minifi::core::ProcessContextBuilder> contextBuilder =
    minifi::core::ClassLoader::getDefaultClassLoader().instantiate<minifi::core::ProcessContextBuilder>("ProcessContextBuilder", "ProcessContextBuilder");
  contextBuilder = contextBuilder->withContentRepository(content_repo_)->withFlowFileRepository(flow_repo_)->withProvider(controller_services_provider_.get())
    ->withProvenanceRepository(prov_repo_)->withConfiguration(configuration_);
  auto context = contextBuilder->build(node);
  processor_contexts_.push_back(context);
  processor_queue_.push_back(processor);
  return processor;
}

std::shared_ptr<minifi::core::Processor> TestPlan::addProcessor(const std::string &processor_name, const minifi::utils::Identifier &uuid, const std::string &name,
    const std::initializer_list<minifi::core::Relationship> &relationships, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  std::shared_ptr<core::CoreComponent> ptr = minifi::core::ClassLoader::getDefaultClassLoader().instantiate(processor_name, uuid);
  if (nullptr == ptr) {
    throw std::runtime_error{fmt::format("Failed to instantiate processor name: {0} uuid: {1}", processor_name, uuid.to_string().c_str())};
  }
  std::shared_ptr<minifi::core::Processor> processor = std::dynamic_pointer_cast<minifi::core::Processor>(ptr);

  processor->setName(name);

  return addProcessor(processor, name, relationships, linkToPrevious);
}

std::shared_ptr<minifi::core::Processor> TestPlan::addProcessor(const std::string &processor_name, const std::string &name, const std::initializer_list<minifi::core::Relationship>& relationships,
    bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);
  return addProcessor(processor_name, minifi::utils::IdGenerator::getIdGenerator()->generate(), name, relationships, linkToPrevious);
}

minifi::Connection* TestPlan::addConnection(const std::shared_ptr<minifi::core::Processor>& source_proc, const minifi::core::Relationship& source_relationship,
    const std::shared_ptr<minifi::core::Processor>& destination_proc) {
  std::stringstream connection_name;
  connection_name
    << (source_proc ? source_proc->getUUIDStr().c_str() : "none")
    << "-to-"
    << (destination_proc ? destination_proc->getUUIDStr().c_str() : "none");
  auto connection = std::make_unique<minifi::ConnectionImpl>(flow_repo_, content_repo_, connection_name.str());

  connection->addRelationship(source_relationship);

  // link the connections so that we can test results at the end for this

  if (source_proc) {
    connection->setSource(source_proc.get());
    connection->setSourceUUID(source_proc->getUUID());
  }
  if (destination_proc) {
    connection->setDestination(destination_proc.get());
    connection->setDestinationUUID(destination_proc->getUUID());
  }
  if (source_proc) {
    source_proc->addConnection(connection.get());
  }
  if (source_proc != destination_proc && destination_proc) {
    destination_proc->addConnection(connection.get());
  }

  auto retVal = connection.get();
  relationships_.push_back(std::move(connection));
  return retVal;
}

std::shared_ptr<minifi::core::controller::ControllerServiceNode> TestPlan::addController(const std::string &controller_name, const std::string &name) {
  if (finalized) {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> guard(mutex);

  minifi::utils::Identifier uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();

  std::shared_ptr<minifi::core::controller::ControllerServiceNode> controller_service_node =
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

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::Processor>& processor, const std::string& property, const std::string& value, bool dynamic) {
  std::lock_guard<std::recursive_mutex> guard(mutex);

  size_t i = 0;
  logger_->log_info("Attempting to set property {} to {} for {}", property, value, processor->getName());
  for (i = 0; i < processor_queue_.size(); i++) {
    if (processor_queue_.at(i) == processor) {
      break;
    }
  }

  if (i >= processor_queue_.size() || i >= processor_contexts_.size()) {
    return false;
  }

  if (dynamic) {
    return processor_contexts_.at(i)->setDynamicProperty(property, value);
  } else {
    return processor_contexts_.at(i)->setProperty(property, value);
  }
}

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::Processor>& processor, const core::PropertyReference& property, std::string_view value) {
  return setProperty(processor, std::string(property.name), std::string(value), false);
}

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::Processor>& processor, std::string_view property, std::string_view value) {
  return setProperty(processor, std::string(property), std::string(value), false);
}

bool TestPlan::setDynamicProperty(const std::shared_ptr<minifi::core::Processor>& processor, std::string_view property, std::string_view value) {
  return setProperty(processor, std::string(property), std::string(value), true);
}

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, const std::string& property, const std::string& value, bool dynamic) {
  if (dynamic) {
    controller_service_node->setDynamicProperty(property, value);
    return controller_service_node->getControllerServiceImplementation()->setDynamicProperty(property, value);
  } else {
    controller_service_node->setProperty(property, value);
    return controller_service_node->getControllerServiceImplementation()->setProperty(property, value);
  }
}

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, const core::PropertyReference& property, std::string_view value) {
  return setProperty(controller_service_node, std::string(property.name), std::string(value), false);
}

bool TestPlan::setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, std::string_view property, std::string_view value) {
  return setProperty(controller_service_node, std::string(property), std::string(value), false);
}

bool TestPlan::setDynamicProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, std::string_view property, std::string_view value) {
  return setProperty(controller_service_node, std::string(property), std::string(value), true);
}

void TestPlan::reset(bool reschedule) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  process_sessions_.clear();
  factories_.clear();
  location = -1;
  if (reschedule)
    configured_processors_.clear();
  for (const auto& proc : processor_queue_) {
    while (proc->getActiveTasks() > 0) {
      proc->decrementActiveTask();
    }
    if (reschedule)
      proc->onUnSchedule();
  }
}

std::vector<std::shared_ptr<minifi::core::Processor>>::iterator TestPlan::getProcessorItByUuid(const std::string& uuid) {
  const auto processor_node_matches_processor = [&uuid] (const std::shared_ptr<minifi::core::Processor>& processor) {
    return processor->getUUIDStr() == uuid;
  };
  auto processor_found_at = std::find_if(processor_queue_.begin(), processor_queue_.end(), processor_node_matches_processor);
  if (processor_found_at == processor_queue_.end()) {
    throw std::runtime_error("Processor not found in test plan.");
  }
  return processor_found_at;
}

std::shared_ptr<minifi::core::ProcessContext> TestPlan::getProcessContextForProcessor(const std::shared_ptr<minifi::core::Processor>& processor) {
  const auto contextMatchesProcessor = [&processor] (const std::shared_ptr<minifi::core::ProcessContext>& context) {
    return context->getProcessorNode()->getUUIDStr() ==  processor->getUUIDStr();
  };
  const auto context_found_at = std::find_if(processor_contexts_.begin(), processor_contexts_.end(), contextMatchesProcessor);
  if (context_found_at == processor_contexts_.end()) {
    throw std::runtime_error("Context not found in test plan.");
  }
  return *context_found_at;
}

void TestPlan::scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const std::shared_ptr<minifi::core::ProcessContext>& context) {
  if (std::find(configured_processors_.begin(), configured_processors_.end(), processor) == configured_processors_.end()) {
    // Ordering on factories and list of configured processors do not matter
    const auto factory = std::make_shared<minifi::core::ProcessSessionFactoryImpl>(context);
    factories_.push_back(factory);
    processor->onSchedule(*context, *factory);
    configured_processors_.push_back(processor);
  }
}

void TestPlan::scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor) {
  scheduleProcessor(processor, getProcessContextForProcessor(processor));
}

void TestPlan::scheduleProcessors() {
  for (std::size_t target_location = 0; target_location < processor_queue_.size(); ++target_location) {
    std::shared_ptr<minifi::core::Processor> processor = processor_queue_.at(target_location);
    std::shared_ptr<minifi::core::ProcessContext> context = processor_contexts_.at(target_location);
    scheduleProcessor(processor, context);
  }
}

bool TestPlan::runProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const PreTriggerVerifier& verify) {
  const auto processor_location = gsl::narrow<size_t>(std::distance(processor_queue_.begin(), getProcessorItByUuid(processor->getUUIDStr())));
  return runProcessor(processor_location, verify);
}

class TestSessionFactory : public minifi::core::ProcessSessionFactoryImpl {
  using SessionCallback = std::function<void(const std::shared_ptr<minifi::core::ProcessSession>&)>;
 public:
  TestSessionFactory(std::shared_ptr<minifi::core::ProcessContext> context, SessionCallback on_new_session)
    : ProcessSessionFactoryImpl(std::move(context)), on_new_session_(std::move(on_new_session)) {}

  std::shared_ptr<minifi::core::ProcessSession> createSession() override {
    auto session = ProcessSessionFactoryImpl::createSession();
    on_new_session_(session);
    return session;
  }

  SessionCallback on_new_session_;
};

bool TestPlan::runProcessor(size_t target_location, const PreTriggerVerifier& verify) {
  if (!finalized) {
    finalize();
  }
  logger_->log_info("Running next processor {}, processor_queue_.size {}, processor_contexts_.size {}", target_location, processor_queue_.size(), processor_contexts_.size());
  std::lock_guard<std::recursive_mutex> guard(mutex);

  std::shared_ptr<minifi::core::Processor> processor = processor_queue_.at(target_location);
  std::shared_ptr<minifi::core::ProcessContext> context = processor_contexts_.at(target_location);
  scheduleProcessor(processor, context);
  current_flowfile_ = nullptr;
  processor->incrementActiveTasks();
  processor->setScheduledState(minifi::core::ScheduledState::RUNNING);

  if (verify) {
    auto current_session = std::make_shared<minifi::core::ProcessSessionImpl>(context);
    process_sessions_.push_back(current_session);
    verify(context, current_session);
    current_session->commit();
  } else {
    auto session_factory = std::make_shared<TestSessionFactory>(context, [&] (auto current_session) {
      process_sessions_.push_back(current_session);
    });
    logger_->log_info("Running {}", processor->getName());
    processor->triggerAndCommit(context, session_factory);
  }

  return gsl::narrow<size_t>(target_location + 1) < processor_queue_.size();
}

bool TestPlan::runNextProcessor(const PreTriggerVerifier& verify) {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  ++location;
  return runProcessor(location, verify);
}

bool TestPlan::runCurrentProcessor() {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  return runProcessor(location);
}

bool TestPlan::runCurrentProcessorUntilFlowfileIsProduced(std::chrono::milliseconds wait_duration) {
  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  const auto isFlowFileProduced = [&] {
    runCurrentProcessor();
    const std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor_queue_.at(location));
    return std::any_of(connections.cbegin(), connections.cend(), [] (const minifi::Connection* conn) { return !conn->isEmpty(); });
  };
  return verifyEventHappenedInPollTime(wait_duration, isFlowFileProduced);
}

std::size_t TestPlan::getNumFlowFileProducedByCurrentProcessor() {
  const auto& processor = processor_queue_.at(gsl::narrow<size_t>(location));
  return getNumFlowFileProducedByProcessor(processor);
}

std::size_t TestPlan::getNumFlowFileProducedByProcessor(const std::shared_ptr<minifi::core::Processor>& processor) {
  std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor);
  std::size_t num_flow_files = 0;
  for (auto connection : connections) {
    num_flow_files += connection->getQueueSize();
  }
  return num_flow_files;
}

std::shared_ptr<minifi::core::FlowFile> TestPlan::getFlowFileProducedByCurrentProcessor() {
  const std::shared_ptr<minifi::core::Processor>& processor = processor_queue_.at(location);
  std::vector<minifi::Connection*> connections = getProcessorOutboundConnections(processor);
  for (auto connection : connections) {
    std::set<std::shared_ptr<minifi::core::FlowFile>> expiredFlowRecords;
    std::shared_ptr<minifi::core::FlowFile> flowfile = connection->poll(expiredFlowRecords);
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

std::set<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> TestPlan::getProvenanceRecords() {
  return process_sessions_.at(location)->getProvenanceReporter()->getEvents();
}

std::shared_ptr<minifi::core::FlowFile> TestPlan::getCurrentFlowFile() {
  if (current_flowfile_ == nullptr) {
    current_flowfile_ = process_sessions_.at(location)->get();
  }
  return current_flowfile_;
}

std::vector<minifi::Connection*> TestPlan::getProcessorOutboundConnections(const std::shared_ptr<minifi::core::Processor>& processor) {
  const auto is_processor_outbound_connection = [&processor] (const std::unique_ptr<minifi::Connection>& connection) {
    // A connection is outbound from a processor if its source uuid matches the processor
    return connection->getSource()->getUUIDStr() == processor->getUUIDStr();
  };
  std::vector<minifi::Connection*> connections;
  for (const auto& relationship : relationships_) {
    if (is_processor_outbound_connection(relationship)) {
      connections.emplace_back(relationship.get());
    }
  }
  return connections;
}


std::shared_ptr<minifi::core::ProcessContext> TestPlan::getCurrentContext() {
  return processor_contexts_.at(location);
}

std::unique_ptr<minifi::Connection> TestPlan::buildFinalConnection(const std::shared_ptr<minifi::core::Processor>& processor, bool setDest) {
  gsl_Expects(termination_);
  std::stringstream connection_name;
  connection_name << processor->getUUIDStr() << "-to-" << processor->getUUIDStr();
  auto connection = std::make_unique<minifi::ConnectionImpl>(flow_repo_, content_repo_, connection_name.str());
  connection->addRelationship(termination_.value());

  // link the connections so that we can test results at the end for this
  connection->setSource(processor.get());
  if (setDest)
    connection->setDestination(processor.get());

  minifi::utils::Identifier uuid_copy = processor->getUUID();
  connection->setSourceUUID(uuid_copy);
  if (setDest)
    connection->setDestinationUUID(uuid_copy);

  processor->addConnection(connection.get());
  return connection;
}

void TestPlan::finalize() {
  std::lock_guard<std::recursive_mutex> guard(mutex);
  if (termination_) {
    if (!relationships_.empty()) {
      relationships_.push_back(buildFinalConnection(processor_queue_.back()));
    } else {
      for (const auto& processor : processor_queue_) {
        relationships_.push_back(buildFinalConnection(processor, true));
      }
    }
  }

  for (auto& controller_service_node : controller_service_nodes_) {
    controller_service_node->enable();
  }

  finalized = true;
}

void TestPlan::validateAnnotations() const {
  for (const auto& processor : processor_queue_) {
    processor->validateAnnotations();
  }
}

std::string TestPlan::getContent(const minifi::core::FlowFile& file) const {
  auto content_claim = file.getResourceClaim();
  auto content_stream = content_repo_->read(*content_claim);
  auto output_stream = std::make_shared<minifi::io::BufferStream>();
  minifi::InputStreamPipe{*output_stream}(content_stream);
  return utils::span_to<std::string>(minifi::utils::as_span<const char>(output_stream->getBuffer()).subspan(file.getOffset(), file.getSize()));
}

TestController::TestController()
    : log(LogTestController::getInstance()) {
  minifi::setDefaultDirectory("./");
  log.reset();
  minifi::utils::IdGenerator::getIdGenerator()->initialize(minifi::Properties::create());
  flow_version_ = std::make_shared<minifi::state::response::FlowVersion>("test", "test", "test");
}

std::shared_ptr<TestPlan> TestController::createPlan(PlanConfig config) {
  if (!config.configuration) {
    config.configuration = minifi::Configure::create();
    config.configuration->setHome(createTempDirectory());
    config.configuration->set(minifi::Configure::nifi_state_storage_local_class_name, "VolatileMapStateStorage");
    config.configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, createTempDirectory().string());
  }

  if (!config.flow_file_repo)
    config.flow_file_repo = std::make_shared<TestRepository>();

  if (!config.content_repo)
    config.content_repo = std::make_shared<minifi::core::repository::VolatileContentRepository>();

  config.content_repo->initialize(config.configuration);
  config.flow_file_repo->initialize(config.configuration);
  config.flow_file_repo->loadComponent(config.content_repo);

  return std::make_shared<TestPlan>(
      std::move(config.content_repo), std::move(config.flow_file_repo), std::make_shared<TestRepository>(),
      flow_version_, config.configuration, config.state_dir ? config.state_dir->string().c_str() : nullptr);
}

std::shared_ptr<TestPlan> TestController::createPlan(std::shared_ptr<minifi::Configure> configuration, std::optional<std::filesystem::path> state_dir,
    std::shared_ptr<minifi::core::ContentRepository> content_repo) {
  return createPlan(PlanConfig{
    .configuration = std::move(configuration),
    .state_dir = std::move(state_dir),
    .content_repo = std::move(content_repo)
  });
}

std::filesystem::path TestController::createTempDirectory() {
  directories.push_back(std::make_unique<TempDirectory>());
  return directories.back()->getPath();
}
