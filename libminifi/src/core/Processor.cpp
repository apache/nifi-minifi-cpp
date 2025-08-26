/**
 * @file Processor.cpp
 * Processor class implementation
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
#include "core/Processor.h"

#include <ctime>
#include <cctype>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "minifi-cpp/Connection.h"
#include "core/Connectable.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/ProcessorConfig.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/ProcessSession.h"
#include "range/v3/algorithm/any_of.hpp"
#include "fmt/format.h"
#include "minifi-cpp/Exception.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

constexpr std::chrono::microseconds MINIMUM_SCHEDULING_PERIOD{30};

static std::mutex& getGraphMutex() {
  static std::mutex mutex{};
  return mutex;
}

Processor::Processor(std::string_view name, std::unique_ptr<ProcessorApi> impl)
    : ConnectableImpl(name),
      state_(DISABLED),
      scheduling_period_(MINIMUM_SCHEDULING_PERIOD),
      run_duration_(DEFAULT_RUN_DURATION),
      yield_period_(DEFAULT_YIELD_PERIOD_SECONDS),
      active_tasks_(0),
      logger_(logging::LoggerFactory<Processor>::getLogger(uuid_)),
      metrics_(gsl::make_not_null(std::make_shared<ProcessorMetrics>(*this))),
      impl_(std::move(impl)) {
  has_work_.store(false);
  // Setup the default values
  strategy_ = TIMER_DRIVEN;
  penalization_period_ = DEFAULT_PENALIZATION_PERIOD;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  incoming_connections_Iter = this->incoming_connections_.begin();
  logger_->log_debug("Processor {} created UUID {}", name_, getUUIDStr());
}

Processor::Processor(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<ProcessorApi> impl)
    : ConnectableImpl(name, uuid),
      state_(DISABLED),
      scheduling_period_(MINIMUM_SCHEDULING_PERIOD),
      run_duration_(DEFAULT_RUN_DURATION),
      yield_period_(DEFAULT_YIELD_PERIOD_SECONDS),
      active_tasks_(0),
      logger_(logging::LoggerFactory<Processor>::getLogger(uuid_)),
      metrics_(gsl::make_not_null(std::make_shared<ProcessorMetrics>(*this))),
      impl_(std::move(impl)) {
  has_work_.store(false);
  // Setup the default values
  strategy_ = TIMER_DRIVEN;
  penalization_period_ = DEFAULT_PENALIZATION_PERIOD;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  incoming_connections_Iter = this->incoming_connections_.begin();
  logger_->log_debug("Processor {} created with uuid {}", name_, getUUIDStr());
}

Processor::~Processor() {
  logger_->log_debug("Destroying processor {} with uuid {}", name_, getUUIDStr());
}

bool Processor::isRunning() const {
  return (state_ == RUNNING && active_tasks_ > 0);
}

void Processor::setScheduledState(ScheduledState state) {
  state_ = state;
  if (state == STOPPED) {
    impl_->notifyStop();
  }
}

bool Processor::addConnection(Connectable* conn) {
  enum class SetAs{
    NONE,
    OUTPUT,
    INPUT,
  };
  SetAs result = SetAs::NONE;

  if (isRunning()) {
    logger_->log_warn("Can not add connection while the process {} is running", name_);
    return false;
  }
  const auto connection = dynamic_cast<Connection*>(conn);
  if (!connection) {
    return false;
  }

  std::lock_guard<std::mutex> lock(getGraphMutex());

  auto updateGraph = gsl::finally([&] {
    if (result == SetAs::INPUT) {
      updateReachability(lock);
    } else if (result == SetAs::OUTPUT) {
      updateReachability(lock, true);
    }
  });

  utils::Identifier srcUUID = connection->getSourceUUID();
  utils::Identifier destUUID = connection->getDestinationUUID();

  if (uuid_ == destUUID) {
    // Connection is destination to the current processor
    if (!incoming_connections_.contains(connection)) {
      incoming_connections_.insert(connection);
      connection->setDestination(this);
      logger_->log_debug("Add connection {} into Processor {} incoming connection", connection->getName(), name_);
      incoming_connections_Iter = this->incoming_connections_.begin();
      result = SetAs::OUTPUT;
    }
  }
  if (uuid_ == srcUUID) {
    for (const auto& rel : connection->getRelationships()) {
      const auto relationship = rel.getName();
      // Connection is source from the current processor
      auto &&it = outgoing_connections_.find(relationship);
      if (it != outgoing_connections_.end()) {
        // We already has connection for this relationship
        std::set<Connectable*> existedConnection = it->second;
        if (!existedConnection.contains(connection)) {
          // We do not have the same connection for this relationship yet
          existedConnection.insert(connection);
          connection->setSource(this);
          outgoing_connections_[relationship] = existedConnection;
          logger_->log_debug("Add connection {} into Processor {} outgoing connection for relationship {}", connection->getName(), name_, relationship);
          result = SetAs::INPUT;
        }
      } else {
        // We do not have any outgoing connection for this relationship yet
        std::set<Connectable*> newConnection;
        newConnection.insert(connection);
        connection->setSource(this);
        outgoing_connections_[relationship] = newConnection;
        logger_->log_debug("Add connection {} into Processor {} outgoing connection for relationship {}", connection->getName(), name_, relationship);
        result = SetAs::INPUT;
      }
    }
  }
  return result != SetAs::NONE;
}

void Processor::triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory) {
  const auto process_session = std::dynamic_pointer_cast<core::ProcessSessionImpl>(session_factory->createSession());
  gsl_Assert(process_session);
  process_session->setMetrics(getMetrics());
  try {
    trigger(context, process_session);
    process_session->commit();
  } catch (const std::exception& exception) {
    logger_->log_warn("Caught \"{}\" ({}) during Processor::onTrigger of processor: {} ({})",
        exception.what(), typeid(exception).name(), getUUIDStr(), getName());
    process_session->rollback();
    throw;
  } catch (...) {
    logger_->log_warn("Caught unknown exception during Processor::onTrigger of processor: {} ({})", getUUIDStr(), getName());
    process_session->rollback();
    throw;
  }
}

void Processor::trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session) {
  ++metrics_->invocations();
  const auto start = std::chrono::steady_clock::now();
  onTrigger(*context, *process_session);
  metrics_->addLastOnTriggerRuntime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
}

bool Processor::isWorkAvailable() {
  // We have work if any incoming connection has work
  std::lock_guard<std::mutex> lock(mutex_);
  bool hasWork = false;

  try {
    for (const auto &conn : incoming_connections_) {
      auto connection = dynamic_cast<Connection*>(conn);
      if (!connection) {
        continue;
      }
      if (connection->isWorkAvailable()) {
        hasWork = true;
        break;
      }
    }
  } catch (...) {
    logger_->log_error("Caught an exception (type: {}) while checking if work is available;"
        " unless it was positively determined that work is available, assuming NO work is available!",
        getCurrentExceptionTypeName());
  }

  return hasWork || impl_->isWorkAvailable();
}

// must hold the graphMutex
void Processor::updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force) {
  bool didChange = force;
  for (auto& outIt : outgoing_connections_) {
    for (auto& outConn : outIt.second) {
      auto connection = dynamic_cast<Connection*>(outConn);
      if (!connection) {
        continue;
      }
      auto dest = dynamic_cast<Processor*>(connection->getDestination());
      if (!dest) {
        continue;
      }
      if (reachable_processors_[connection].insert(dest).second) {
        didChange = true;
      }
      for (auto& reachedIt : dest->reachable_processors()) {
        for (auto &reached_proc : reachedIt.second) {
          if (reachable_processors_[connection].insert(reached_proc).second) {
            didChange = true;
          }
        }
      }
    }
  }
  if (didChange) {
    // propagate the change to sources
    for (auto& inConn : incoming_connections_) {
      auto connection = dynamic_cast<Connection*>(inConn);
      if (!connection) {
        continue;
      }
      auto source = dynamic_cast<Processor*>(connection->getSource());
      if (!source) {
        continue;
      }
      source->updateReachability(graph_lock);
    }
  }
}

bool Processor::partOfCycle(Connection* conn) {
  auto source = dynamic_cast<Processor*>(conn->getSource());
  if (!source) {
    return false;
  }
  auto it = source->reachable_processors().find(conn);
  if (it == source->reachable_processors().end()) {
    return false;
  }
  return it->second.contains(source);
}

bool Processor::isThrottledByBackpressure() const {
  bool isThrottledByOutgoing = ranges::any_of(outgoing_connections_, [](auto& name_connection_set_pair) {
    return ranges::any_of(name_connection_set_pair.second, [](auto& connectable) {
      auto connection = dynamic_cast<Connection*>(connectable);
      return connection && connection->backpressureThresholdReached();
    });
  });
  bool isForcedByIncomingCycle = ranges::any_of(incoming_connections_, [](auto& connectable) {
    auto connection = dynamic_cast<Connection*>(connectable);
    return connection && partOfCycle(connection) && connection->backpressureThresholdReached();
  });
  return isThrottledByOutgoing && !isForcedByIncomingCycle;
}

Connectable* Processor::pickIncomingConnection() {
  std::lock_guard<std::mutex> rel_guard(relationship_mutex_);

  auto beginIt = incoming_connections_Iter;
  Connectable* inConn = nullptr;
  do {
    inConn = getNextIncomingConnectionImpl(rel_guard);
    auto connection = dynamic_cast<Connection*>(inConn);
    if (!connection) {
      continue;
    }
    if (partOfCycle(connection) && connection->backpressureThresholdReached()) {
      return inConn;
    }
  } while (incoming_connections_Iter != beginIt);

  // we did not find a preferred connection
  return getNextIncomingConnectionImpl(rel_guard);
}

void Processor::validateAnnotations() const {
  switch (getInputRequirement()) {
    case annotation::Input::INPUT_REQUIRED: {
      if (!hasIncomingConnections()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("INPUT_REQUIRED was specified for the processor '{}' (uuid: '{}'), but no incoming connections were found",
          getName(), std::string(getUUIDStr())));
      }
      break;
    }
    case annotation::Input::INPUT_ALLOWED:
      break;
    case annotation::Input::INPUT_FORBIDDEN: {
      if (hasIncomingConnections()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("INPUT_FORBIDDEN was specified for the processor '{}' (uuid: '{}'), but there are incoming connections",
          getName(), std::string(getUUIDStr())));
      }
    }
  }
}

void Processor::setMaxConcurrentTasks(const uint8_t tasks) {
  if (isSingleThreaded() && tasks > 1) {
    logger_->log_warn("Processor {} can not be run in parallel, its \"max concurrent tasks\" value is too high. "
                      "It was set to 1 from {}.", name_, tasks);
    max_concurrent_tasks_ = 1;
    return;
  }

  max_concurrent_tasks_ = tasks;
}

void Processor::yield() {
  yield_expiration_ = std::chrono::steady_clock::now() + yield_period_.load();
}

void Processor::yield(std::chrono::steady_clock::duration delta_time) {
  yield_expiration_ = std::chrono::steady_clock::now() + delta_time;
}

bool Processor::isYield() const {
  return getYieldTime() > 0ms;
}

void Processor::clearYield() {
  yield_expiration_ = std::chrono::steady_clock::time_point();
}

std::chrono::steady_clock::duration Processor::getYieldTime() const {
  return std::max(yield_expiration_.load()-std::chrono::steady_clock::now(), std::chrono::steady_clock::duration{0});
}

namespace {

class ProcessorDescriptorImpl : public ProcessorDescriptor {
 public:
  explicit ProcessorDescriptorImpl(Processor* impl): impl_(impl) {}
  void setSupportedRelationships(std::span<const RelationshipDefinition> relationships) override {
    impl_->setSupportedRelationships(relationships);
  }

  void setSupportedProperties(std::span<const PropertyReference> properties) override {
    impl_->setSupportedProperties(properties);
  }

  void setSupportedProperties(std::span<const Property> properties) override {
    impl_->setSupportedProperties(properties);
  }

 private:
  Processor* impl_;
};

}  // namespace

void Processor::initialize() {
  ProcessorDescriptorImpl self{this};
  impl_->initialize(self);
}

ScheduledState Processor::getScheduledState() const {
  return state_;
}

void Processor::setSchedulingStrategy(SchedulingStrategy strategy) {
  strategy_ = strategy;
}

SchedulingStrategy Processor::getSchedulingStrategy() const {
  return strategy_;
}

void Processor::setSchedulingPeriod(std::chrono::steady_clock::duration period) {
  scheduling_period_ = std::max(std::chrono::steady_clock::duration(MINIMUM_SCHEDULING_PERIOD), period);
}

std::chrono::steady_clock::duration Processor::getSchedulingPeriod() const {
  return scheduling_period_;
}

void Processor::setCronPeriod(const std::string &period) {
  cron_period_ = period;
}

std::string Processor::getCronPeriod() const {
  return cron_period_;
}

void Processor::setRunDurationNano(std::chrono::steady_clock::duration period) {
  run_duration_ = period;
}

std::chrono::steady_clock::duration Processor::getRunDurationNano() const {
  return (run_duration_);
}

void Processor::setYieldPeriodMsec(std::chrono::milliseconds period) {
  yield_period_ = period;
}

std::chrono::steady_clock::duration Processor::getYieldPeriod() const {
  return yield_period_;
}

void Processor::setPenalizationPeriod(std::chrono::milliseconds period) {
  penalization_period_ = period;
}

bool Processor::isSingleThreaded() const {
  return impl_->isSingleThreaded();
}

std::string Processor::getProcessorType() const {
  return impl_->getProcessorType();
}

bool Processor::getTriggerWhenEmpty() const {
  return impl_->getTriggerWhenEmpty();
}

uint8_t Processor::getActiveTasks() const {
  return (active_tasks_);
}

void Processor::incrementActiveTasks() {
  ++active_tasks_;
}

void Processor::decrementActiveTask() {
  if (active_tasks_ > 0)
    --active_tasks_;
}

void Processor::clearActiveTask() {
  active_tasks_ = 0;
}

std::string Processor::getProcessGroupUUIDStr() const {
  return process_group_uuid_;
}

void Processor::setProcessGroupUUIDStr(const std::string &uuid) {
  process_group_uuid_ = uuid;
}

std::string Processor::getProcessGroupName() const {
  return process_group_name_;
}

void Processor::setProcessGroupName(const std::string &name) {
  process_group_name_ = name;
}

std::string Processor::getProcessGroupPath() const {
  return process_group_path_;
}

void Processor::setProcessGroupPath(const std::string &path) {
  process_group_path_ = path;
}

logging::LOG_LEVEL Processor::getLogBulletinLevel() const {
  return log_bulletin_level_;
}

void Processor::setLogBulletinLevel(logging::LOG_LEVEL level) {
  log_bulletin_level_ = level;
}

void Processor::setLoggerCallback(const std::function<void(logging::LOG_LEVEL level, const std::string& message)>& callback) {
  impl_->forEachLogger([&] (std::shared_ptr<logging::Logger> logger) {
    std::dynamic_pointer_cast<logging::LoggerBase>(logger)->setLogCallback(callback);
  });
}

std::chrono::steady_clock::time_point Processor::getYieldExpirationTime() const {
  return yield_expiration_;
}

bool Processor::canEdit() {
  return !isRunning();
}

void Processor::onTrigger(ProcessContext& context, ProcessSession& session) {
  impl_->onTrigger(context, session);
}

void Processor::onSchedule(ProcessContext& context, ProcessSessionFactory& session_factory) {
  impl_->onSchedule(context, session_factory);
}

// Hook executed when onSchedule fails (throws). Configuration should be reset in this
void Processor::onUnSchedule() {
  impl_->onUnSchedule();
}

annotation::Input Processor::getInputRequirement() const {
  return impl_->getInputRequirement();
}

[[nodiscard]] bool Processor::supportsDynamicProperties() const {
  return impl_->supportsDynamicProperties();
}

[[nodiscard]] bool Processor::supportsDynamicRelationships() const {
  return impl_->supportsDynamicRelationships();
}

state::response::SharedResponseNode Processor::getResponseNode() {
  return metrics_;
}

gsl::not_null<std::shared_ptr<ProcessorMetrics>> Processor::getMetrics() const {
  return metrics_;
}

std::shared_ptr<ProcessorMetricsExtension> Processor::getMetricsExtension() const {
  return impl_->getMetricsExtension();
}

void Processor::restore(const std::shared_ptr<FlowFile>& file) {
  impl_->restore(file);
}

const std::unordered_map<Connection*, std::unordered_set<Processor*>>& Processor::reachable_processors() const {
  return reachable_processors_;
}

}  // namespace org::apache::nifi::minifi::core
