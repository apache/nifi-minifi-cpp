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

#include "c2/C2Agent.h"

#include <cinttypes>
#include <cstdio>
#include <csignal>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "c2/ControllerSocketProtocol.h"
#include "core/ProcessContext.h"
#include "core/CoreComponentState.h"
#include "core/state/UpdateController.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileManager.h"
#include "utils/file/FileSystem.h"
#include "utils/HTTPClient.h"
#include "utils/Environment.h"
#include "utils/Monitors.h"
#include "utils/StringUtils.h"
#include "io/ArchiveStream.h"
#include "io/StreamPipe.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::c2 {

C2Agent::C2Agent(core::controller::ControllerServiceProvider *controller,
                 state::Pausable *pause_handler,
                 state::StateMonitor* updateSink,
                 std::shared_ptr<Configure> configuration,
                 std::shared_ptr<utils::file::FileSystem> filesystem,
                 std::function<void()> request_restart)
    : heart_beat_period_(3s),
      max_c2_responses(5),
      update_sink_(updateSink),
      update_service_(nullptr),
      controller_(controller),
      pause_handler_(pause_handler),
      configuration_(std::move(configuration)),
      filesystem_(std::move(filesystem)),
      protocol_(nullptr),
      thread_pool_(2, false, nullptr, "C2 threadpool"),
      request_restart_(std::move(request_restart)) {
  manifest_sent_ = false;

  last_run_ = std::chrono::steady_clock::now();

  if (nullptr != controller_) {
    update_service_ = std::static_pointer_cast<controllers::UpdatePolicyControllerService>(controller_->getControllerService(UPDATE_NAME));
  }

  if (update_service_ == nullptr) {
    // create a stubbed service for updating the flow identifier
  }

  configure(configuration_, false);

  functions_.emplace_back([this] {return produce();});
  functions_.emplace_back([this] {return consume();});
}

void C2Agent::start() {
  if (controller_running_) {
    return;
  }
  task_ids_.clear();
  for (const auto& function : functions_) {
    utils::Identifier uuid = utils::IdGenerator::getIdGenerator()->generate();
    task_ids_.push_back(uuid);
    utils::Worker<utils::TaskRescheduleInfo> functor(function, uuid.to_string(), std::make_unique<utils::ComplexMonitor>());
    std::future<utils::TaskRescheduleInfo> future;
    thread_pool_.execute(std::move(functor), future);
  }
  controller_running_ = true;
  thread_pool_.start();
  logger_->log_info("C2 agent started");
}

void C2Agent::stop() {
  controller_running_ = false;
  for (const auto& id : task_ids_) {
    thread_pool_.stopTasks(id.to_string());
  }
  thread_pool_.shutdown();
  logger_->log_info("C2 agent stopped");
}

void C2Agent::checkTriggers() {
  logger_->log_debug("Checking %d triggers", triggers_.size());
  for (const auto &trigger : triggers_) {
    if (trigger->triggered()) {
      /**
       * Action was triggered, so extract it.
       */
      C2Payload &&triggerAction = trigger->getAction();
      logger_->log_trace("%s action triggered", trigger->getName());
      // handle the response the same way. This means that
      // acknowledgements will be sent to the c2 server for every trigger action.
      // this is expected
      extractPayload(std::move(triggerAction));
      // call reset if the trigger supports this activity
      trigger->reset();
    } else {
      logger_->log_trace("%s action not triggered", trigger->getName());
    }
  }
}
void C2Agent::configure(const std::shared_ptr<Configure> &configure, bool reconfigure) {
  std::string clazz, heartbeat_period, device;

  if (!reconfigure) {
    if (!configure->get(Configuration::nifi_c2_agent_protocol_class, "c2.agent.protocol.class", clazz)) {
      clazz = "RESTSender";
    }
    logger_->log_info("Class is %s", clazz);

    auto protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw(clazz, clazz);
    if (protocol == nullptr) {
      logger_->log_warn("Class %s not found", clazz);
      protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw("RESTSender", "RESTSender");
      if (!protocol) {
        const char* errmsg = "Attempted to load RESTSender. To enable C2, please specify an active protocol for this agent.";
        logger_->log_error(errmsg);
        throw Exception{ GENERAL_EXCEPTION, errmsg };
      }

      logger_->log_info("Class is RESTSender");
    }

    // Since !reconfigure, the call comes from the ctor and protocol_ is null, therefore no delete is necessary
    protocol_.exchange(dynamic_cast<C2Protocol *>(protocol));

    protocol_.load()->initialize(controller_, configuration_);
  } else {
    protocol_.load()->update(configure);
  }

  if (configure->get(Configuration::nifi_c2_agent_heartbeat_period, "c2.agent.heartbeat.period", heartbeat_period)) {
    try {
      if (auto heartbeat_period_ms = utils::timeutils::StringToDuration<std::chrono::milliseconds>(heartbeat_period)) {
        heart_beat_period_ = *heartbeat_period_ms;
        logger_->log_debug("Using %u ms as the heartbeat period", heart_beat_period_.count());
      } else {
        heart_beat_period_ = std::chrono::milliseconds(std::stoi(heartbeat_period));
      }
    } catch (const std::invalid_argument &) {
      heart_beat_period_ = 3s;
    }
  } else {
    if (!reconfigure)
      heart_beat_period_ = 3s;
  }

  std::string heartbeat_reporters;
  if (configure->get(Configuration::nifi_c2_agent_heartbeat_reporter_classes, "c2.agent.heartbeat.reporter.classes", heartbeat_reporters)) {
    std::vector<std::string> reporters = utils::StringUtils::splitAndTrim(heartbeat_reporters, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& reporter : reporters) {
      auto heartbeat_reporter_obj = core::ClassLoader::getDefaultClassLoader().instantiate<HeartbeatReporter>(reporter, reporter);
      if (heartbeat_reporter_obj == nullptr) {
        logger_->log_error("Could not instantiate %s", reporter);
      } else {
        heartbeat_reporter_obj->initialize(controller_, update_sink_, configuration_);
        heartbeat_protocols_.push_back(std::move(heartbeat_reporter_obj));
      }
    }
  }

  std::string trigger_classes;
  if (configure->get(Configuration::nifi_c2_agent_trigger_classes, "c2.agent.trigger.classes", trigger_classes)) {
    std::vector<std::string> triggers = utils::StringUtils::splitAndTrim(trigger_classes, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& trigger : triggers) {
      auto trigger_obj = core::ClassLoader::getDefaultClassLoader().instantiate<C2Trigger>(trigger, trigger);
      if (trigger_obj == nullptr) {
        logger_->log_error("Could not instantiate %s", trigger);
      } else {
        trigger_obj->initialize(configuration_);
        triggers_.push_back(std::move(trigger_obj));
      }
    }
  }

  std::string base_reporter = "ControllerSocketProtocol";
  auto heartbeat_reporter_obj = core::ClassLoader::getDefaultClassLoader().instantiate<HeartbeatReporter>(base_reporter, base_reporter);
  if (heartbeat_reporter_obj == nullptr) {
    logger_->log_error("Could not instantiate %s", base_reporter);
  } else {
    heartbeat_reporter_obj->initialize(controller_, update_sink_, configuration_);
    heartbeat_protocols_.push_back(std::move(heartbeat_reporter_obj));
  }
}

void C2Agent::performHeartBeat() {
  C2Payload payload(Operation::HEARTBEAT);
  logger_->log_trace("Performing heartbeat");
  auto reporter = dynamic_cast<state::response::NodeReporter*>(update_sink_);
  std::vector<state::response::NodeReporter::ReportedNode> metrics;
  if (reporter) {
    if (!manifest_sent_) {
      // include agent manifest for the first heartbeat
      metrics = reporter->getHeartbeatNodes(true);
      manifest_sent_ = true;
    } else {
      metrics = reporter->getHeartbeatNodes(false);
    }

    payload.reservePayloads(metrics.size());
    for (const auto& metric : metrics) {
      C2Payload child_metric_payload(Operation::HEARTBEAT);
      child_metric_payload.setLabel(metric.name);
      child_metric_payload.setContainer(metric.is_array);
      serializeMetrics(child_metric_payload, metric.name, metric.serialized_nodes, metric.is_array);
      payload.addPayload(std::move(child_metric_payload));
    }
  }
  C2Payload response = protocol_.load()->consumePayload(payload);

  enqueue_c2_server_response(std::move(response));

  std::lock_guard<std::mutex> lock(heartbeat_mutex);

  for (auto& reporter : heartbeat_protocols_) {
    reporter->heartbeat(payload);
  }
}

void C2Agent::serializeMetrics(C2Payload &metric_payload, const std::string &name, const std::vector<state::response::SerializedResponseNode> &metrics, bool is_container, bool is_collapsible) {
  const auto payloads = std::count_if(begin(metrics), end(metrics), [](const state::response::SerializedResponseNode& metric) { return !metric.children.empty() || metric.keep_empty; });
  metric_payload.reservePayloads(metric_payload.getNestedPayloads().size() + payloads);
  for (const auto &metric : metrics) {
    if (metric.children.size() > 0 || (metric.children.size() == 0 && metric.keep_empty)) {
      C2Payload child_metric_payload(metric_payload.getOperation());
      if (metric.array) {
        child_metric_payload.setContainer(true);
      }
      auto collapsible = !metric.collapsible ? metric.collapsible : is_collapsible;
      child_metric_payload.setCollapsible(collapsible);
      child_metric_payload.setLabel(metric.name);
      serializeMetrics(child_metric_payload, metric.name, metric.children, is_container, collapsible);
      metric_payload.addPayload(std::move(child_metric_payload));
    } else {
      C2ContentResponse response(metric_payload.getOperation());
      response.name = name;
      response.operation_arguments[metric.name] = metric.value;
      metric_payload.addContent(std::move(response), is_collapsible);
    }
  }
}

void C2Agent::extractPayload(const C2Payload &resp) {
  if (resp.getStatus().getState() == state::UpdateState::NESTED) {
    for (const C2Payload& payload : resp.getNestedPayloads()) {
      extractPayload(payload);
    }
    return;
  }
  switch (resp.getStatus().getState()) {
    case state::UpdateState::INITIATE:
      logger_->log_trace("Received initiation event from protocol");
      break;
    case state::UpdateState::READ_COMPLETE:
      logger_->log_trace("Received Ack from Server");
      // we have a heartbeat response.
      for (const C2ContentResponse &server_response : resp.getContent()) {
        handle_c2_server_response(server_response);
      }
      break;
    case state::UpdateState::FULLY_APPLIED:
      logger_->log_trace("Received fully applied event from protocol");
      break;
    case state::UpdateState::NESTED:  // multiple updates embedded into one
      logger_->log_trace("Received nested event from protocol");
      break;
    case state::UpdateState::PARTIALLY_APPLIED:
      logger_->log_debug("Received partially applied event from protocol");
      break;
    case state::UpdateState::NOT_APPLIED:
      logger_->log_debug("Received not applied event from protocol");
      break;
    case state::UpdateState::SET_ERROR:
      logger_->log_debug("Received set error event from protocol");
      break;
    case state::UpdateState::READ_ERROR:
      logger_->log_debug("Received read error event from protocol");
      break;
    case state::UpdateState::NO_OPERATION:
      logger_->log_debug("Received no operation event from protocol");
      break;
    default:
      logger_->log_error("Received unknown event (%d) from protocol", static_cast<int>(resp.getStatus().getState()));
      break;
  }
}

namespace {

struct C2TransferError : public std::runtime_error {
  using runtime_error::runtime_error;
};

struct C2DebugBundleError : public C2TransferError {
  using C2TransferError::C2TransferError;
};

}  // namespace

void C2Agent::handle_c2_server_response(const C2ContentResponse &resp) {
  switch (resp.op.value()) {
    case Operation::CLEAR:
      handle_clear(resp);
      break;
    case Operation::UPDATE:
      handle_update(resp);
      break;
    case Operation::DESCRIBE:
      handle_describe(resp);
      break;
    case Operation::RESTART: {
      update_sink_->stop();
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
      protocol_.load()->consumePayload(std::move(response));
      restart_needed_ = true;
    }
      break;
    case Operation::START:
    case Operation::STOP: {
      if (resp.name == "C2" || resp.name == "c2") {
        raise(SIGTERM);
      }

      // stop all referenced components.
      update_sink_->executeOnComponent(resp.name, [this, &resp] (state::StateController& component) {
        logger_->log_debug("Stopping component %s", component.getComponentName());
        if (resp.op == Operation::STOP) {
          component.stop();
        } else {
          component.start();
        }
      });

      if (resp.ident.length() > 0) {
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
        enqueue_c2_response(std::move(response));
      }
    }
      //
      break;
    case Operation::PAUSE:
      if (pause_handler_ != nullptr) {
        pause_handler_->pause();
      } else {
        logger_->log_warn("Pause functionality is not supported!");
      }
      break;
    case Operation::RESUME:
      if (pause_handler_ != nullptr) {
        pause_handler_->resume();
      } else {
        logger_->log_warn("Resume functionality is not supported!");
      }
      break;
    case Operation::TRANSFER: {
      try {
        handle_transfer(resp);
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
        enqueue_c2_response(std::move(response));
      } catch (const std::runtime_error& error) {
        C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, true);
        response.setRawData(error.what());
        enqueue_c2_response(std::move(response));
      }
      break;
    }
    default:
      break;
      // do nothing
  }
}

C2Payload C2Agent::prepareConfigurationOptions(const C2ContentResponse &resp) const {
    auto unsanitized_keys = configuration_->getConfiguredKeys();
    std::vector<std::string> keys;
    std::copy_if(unsanitized_keys.begin(), unsanitized_keys.end(), std::back_inserter(keys),
            [](std::string key) {return key.find("pass") == std::string::npos;});

    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
    C2Payload options(Operation::ACKNOWLEDGE);
    options.setLabel("configuration_options");
    std::string value;
    for (const auto& key : keys) {
      if (configuration_->get(key, value)) {
        C2ContentResponse option(Operation::ACKNOWLEDGE);
        option.name = key;
        option.operation_arguments[key] = value;
        options.addContent(std::move(option));
      }
    }
    response.addPayload(std::move(options));
    return response;
}

void C2Agent::handle_clear(const C2ContentResponse &resp) {
  ClearOperand operand;
  try {
    operand = ClearOperand::parse(resp.name.c_str());
  } catch(const std::runtime_error&) {
    logger_->log_debug("Clearing unknown %s", resp.name);
    return;
  }

  switch (operand.value()) {
    case ClearOperand::CONNECTION: {
      for (const auto& connection : resp.operation_arguments) {
        logger_->log_debug("Clearing connection %s", connection.second.to_string());
        update_sink_->clearConnection(connection.second.to_string());
      }
      break;
    }
    case ClearOperand::REPOSITORIES: {
      update_sink_->drainRepositories();
      break;
    }
    case ClearOperand::CORECOMPONENTSTATE: {
      for (const auto& corecomponent : resp.operation_arguments) {
        auto state_manager_provider = core::ProcessContext::getStateManagerProvider(logger_, controller_, configuration_);
        if (state_manager_provider != nullptr) {
          update_sink_->executeOnComponent(corecomponent.second.to_string(), [this, &state_manager_provider] (state::StateController& component) {
            logger_->log_debug("Clearing state for component %s", component.getComponentName());
            auto state_manager = state_manager_provider->getCoreComponentStateManager(component.getComponentUUID());
            if (state_manager != nullptr) {
              component.stop();
              state_manager->clear();
              state_manager->persist();
              component.start();
            } else {
              logger_->log_warn("Failed to get StateManager for component %s", component.getComponentUUID().to_string());
            }
          });
        } else {
          logger_->log_error("Failed to get StateManagerProvider");
        }
      }
      break;
    }
    default:
      logger_->log_error("Unknown clear operand %s", resp.name);
  }

  C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
  enqueue_c2_response(std::move(response));
}

/**
 * Descriptions are special types of requests that require information
 * to be put into the acknowledgement
 */
void C2Agent::handle_describe(const C2ContentResponse &resp) {
  DescribeOperand operand;
  try {
    operand = DescribeOperand::parse(resp.name.c_str());
  } catch(const std::runtime_error&) {
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  auto reporter = dynamic_cast<state::response::NodeReporter*>(update_sink_);
  switch (operand.value()) {
    case DescribeOperand::METRICS: {
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
      if (reporter != nullptr) {
        auto iter = resp.operation_arguments.find("metricsClass");
        std::string metricsClass;
        if (iter != resp.operation_arguments.end()) {
          metricsClass = iter->second.to_string();
        }
        auto metricsNode = reporter->getMetricsNode(metricsClass);
        C2Payload metrics(Operation::ACKNOWLEDGE);
        metricsClass.empty() ? metrics.setLabel("metrics") : metrics.setLabel(metricsClass);
        if (metricsNode) {
          serializeMetrics(metrics, metricsNode->name, metricsNode->serialized_nodes, metricsNode->is_array);
        }
        response.addPayload(std::move(metrics));
      }
      enqueue_c2_response(std::move(response));
      break;
    }
    case DescribeOperand::CONFIGURATION: {
      auto configOptions = prepareConfigurationOptions(resp);
      enqueue_c2_response(std::move(configOptions));
      break;
    }
    case DescribeOperand::MANIFEST: {
      C2Payload response(prepareConfigurationOptions(resp));
      if (reporter != nullptr) {
        C2Payload agentInfo(Operation::ACKNOWLEDGE, resp.ident, true);
        agentInfo.setLabel("agentInfo");

        const auto manifest = reporter->getAgentManifest();
        serializeMetrics(agentInfo, manifest.name, manifest.serialized_nodes);
        response.addPayload(std::move(agentInfo));
      }
      enqueue_c2_response(std::move(response));
      break;
    }
    case DescribeOperand::JSTACK: {
      if (update_sink_->isRunning()) {
        const std::vector<BackTrace> traces = update_sink_->getTraces();
        for (const auto &trace : traces) {
          for (const auto & line : trace.getTraces()) {
            logger_->log_trace("%s -- %s", trace.getName(), line);
          }
        }
        auto keys = configuration_->getConfiguredKeys();
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
        for (const auto &trace : traces) {
          C2Payload options(Operation::ACKNOWLEDGE, resp.ident, true);
          options.setLabel(trace.getName());
          std::string value;
          for (const auto &line : trace.getTraces()) {
            C2ContentResponse option(Operation::ACKNOWLEDGE);
            option.name = line;
            option.operation_arguments[line] = line;
            options.addContent(std::move(option));
          }
          response.addPayload(std::move(options));
        }
        enqueue_c2_response(std::move(response));
      }
      break;
    }
    case DescribeOperand::CORECOMPONENTSTATE: {
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, true);
      response.setLabel("corecomponentstate");
      C2Payload states(Operation::ACKNOWLEDGE, resp.ident, true);
      states.setLabel("corecomponentstate");
      auto state_manager_provider = core::ProcessContext::getStateManagerProvider(logger_, controller_, configuration_);
      if (state_manager_provider != nullptr) {
        auto core_component_states = state_manager_provider->getAllCoreComponentStates();
        for (const auto& core_component_state : core_component_states) {
          C2Payload state(Operation::ACKNOWLEDGE, resp.ident, true);
          state.setLabel(core_component_state.first.to_string());
          for (const auto& kv : core_component_state.second) {
            C2ContentResponse entry(Operation::ACKNOWLEDGE);
            entry.name = kv.first;
            entry.operation_arguments[kv.first] = kv.second;
            state.addContent(std::move(entry));
          }
          states.addPayload(std::move(state));
        }
      }
      response.addPayload(std::move(states));
      enqueue_c2_response(std::move(response));
      break;
    }
  }
}

void C2Agent::handle_update(const C2ContentResponse &resp) {
  UpdateOperand operand;
  try {
    operand = UpdateOperand::parse(resp.name.c_str());
  } catch(const std::runtime_error&) {
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::NOT_APPLIED, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  switch (operand.value()) {
    case UpdateOperand::CONFIGURATION: {
      handleConfigurationUpdate(resp);
      break;
    }
    case UpdateOperand::PROPERTIES: {
      handlePropertyUpdate(resp);
      break;
    }
    case UpdateOperand::ASSET: {
      handleAssetUpdate(resp);
      break;
    }
  }
}

void C2Agent::handlePropertyUpdate(const C2ContentResponse &resp) {
  state::UpdateState result = state::UpdateState::NO_OPERATION;
  auto changeUpdateState = [&result](UpdateResult update_result) {
    if (result == state::UpdateState::NO_OPERATION) {
      if (update_result == UpdateResult::UPDATE_SUCCESSFUL) {
        result = state::UpdateState::FULLY_APPLIED;
      } else if (update_result == UpdateResult::UPDATE_FAILED) {
        result = state::UpdateState::NOT_APPLIED;
      }
    } else if ((result == state::UpdateState::FULLY_APPLIED && update_result == UpdateResult::UPDATE_FAILED) ||
               (result == state::UpdateState::NOT_APPLIED && update_result == UpdateResult::UPDATE_SUCCESSFUL)) {
      result = state::UpdateState::PARTIALLY_APPLIED;
    }
  };

  for (auto entry : resp.operation_arguments) {
    bool persist = (
        entry.second.getAnnotation("persist")
        | utils::map(&AnnotatedValue::to_string)
        | utils::flatMap(utils::StringUtils::toBool)).value_or(true);
    PropertyChangeLifetime lifetime = persist ? PropertyChangeLifetime::PERSISTENT : PropertyChangeLifetime::TRANSIENT;
    changeUpdateState(update_property(entry.first, entry.second.to_string(), lifetime));
  }
  // apply changes and persist properties requested to be persisted
  const bool propertyWasUpdated = result == state::UpdateState::FULLY_APPLIED || result == state::UpdateState::PARTIALLY_APPLIED;
  if (propertyWasUpdated && !configuration_->commitChanges()) {
    result = state::UpdateState::PARTIALLY_APPLIED;
  }
  C2Payload response(Operation::ACKNOWLEDGE, result, resp.ident, true);
  enqueue_c2_response(std::move(response));
  if (propertyWasUpdated) { restart_needed_ = true; }
}

/**
 * Updates a property
 */
C2Agent::UpdateResult C2Agent::update_property(const std::string &property_name, const std::string &property_value, PropertyChangeLifetime lifetime) {
  if (!Configuration::validatePropertyValue(property_name, property_value) ||
      (update_service_ && !update_service_->canUpdate(property_name))) {
    return UpdateResult::UPDATE_FAILED;
  }

  std::string value;
  if (configuration_->get(property_name, value) && value == property_value) {
    return UpdateResult::NO_UPDATE;
  }

  configuration_->set(property_name, property_value, lifetime);
  return UpdateResult::UPDATE_SUCCESSFUL;
}

C2Payload C2Agent::bundleDebugInfo(std::map<std::string, std::unique_ptr<io::InputStream>>& files) {
  C2Payload payload(Operation::TRANSFER, false);
  auto stream_provider = core::ClassLoader::getDefaultClassLoader().instantiate<io::ArchiveStreamProvider>(
      "ArchiveStreamProvider", "ArchiveStreamProvider");
  if (!stream_provider) {
    throw C2DebugBundleError("Couldn't instantiate archiver provider");
  }
  auto bundle = std::make_shared<io::BufferStream>();
  auto archiver = stream_provider->createWriteStream(9, "gzip", bundle, logger_);
  if (!archiver) {
    throw C2DebugBundleError("Couldn't instantiate archiver");
  }
  for (auto&[filename, stream] : files) {
    size_t file_size = stream->size();
    if (!archiver->newEntry({filename, file_size})) {
      throw C2DebugBundleError("Couldn't initialize archive entry for '" + filename + "'");
    }
    if (gsl::narrow<int64_t>(file_size) != internal::pipe(*stream, *archiver)) {
      // we have touched the input streams, they cannot be reused
      throw C2DebugBundleError("Error while writing file '" + filename + "' into the debug bundle");
    }
  }
  if (!archiver->finish()) {
    throw C2DebugBundleError("Failed to complete debug bundle archive");
  }
  C2Payload file(Operation::TRANSFER, true);
  file.setLabel("debug.tar.gz");
  file.setRawData(bundle->moveBuffer());
  payload.addPayload(std::move(file));
  return payload;
}

void C2Agent::handle_transfer(const C2ContentResponse &resp) {
  TransferOperand operand;
  try {
    operand = TransferOperand::parse(resp.name.c_str());
  } catch(const std::runtime_error&) {
    throw C2TransferError("Unknown operand '" + resp.name + "'");
  }

  switch (operand.value()) {
    case TransferOperand::DEBUG: {
      auto target_it = resp.operation_arguments.find("target");
      if (target_it == resp.operation_arguments.end()) {
        throw C2DebugBundleError("Missing argument for debug operation: 'target'");
      }
      std::optional<std::string> url = resolveUrl(target_it->second.to_string());
      if (!url) {
        throw C2DebugBundleError("Invalid url");
      }
      std::map<std::string, std::unique_ptr<io::InputStream>> files = update_sink_->getDebugInfo();

      auto bundle = bundleDebugInfo(files);
      C2Payload &&response = protocol_.load()->consumePayload(url.value(), bundle, TRANSMIT, false);
      if (response.getStatus().getState() == state::UpdateState::READ_ERROR) {
        throw C2DebugBundleError("Error while uploading");
      }
      break;
    }
  }
}

utils::TaskRescheduleInfo C2Agent::produce() {
  // place priority on messages to send to the c2 server
  if (protocol_.load() != nullptr) {
    std::vector<C2Payload> payload_batch;
    payload_batch.reserve(max_c2_responses);
    auto getRequestPayload = [&payload_batch] (C2Payload&& payload) { payload_batch.emplace_back(std::move(payload)); };
    for (std::size_t attempt_num = 0; attempt_num < max_c2_responses; ++attempt_num) {
      if (!requests.consume(getRequestPayload)) {
        break;
      }
    }
    std::for_each(
        std::make_move_iterator(payload_batch.begin()),
        std::make_move_iterator(payload_batch.end()),
        [&] (C2Payload&& payload) {
          try {
            C2Payload && response = protocol_.load()->consumePayload(std::move(payload));
            enqueue_c2_server_response(std::move(response));
          }
          catch(const std::exception &e) {
            logger_->log_error("Exception occurred while consuming payload. error: %s", e.what());
          }
          catch(...) {
            logger_->log_error("Unknown exception occurred while consuming payload.");
          }
        });

    if (restart_needed_ && requests.empty()) {
      configuration_->commitChanges();
      request_restart_();
      return utils::TaskRescheduleInfo::Done();
    }

    try {
      performHeartBeat();
    }
    catch (const std::exception &e) {
      logger_->log_error("Exception occurred while performing heartbeat. error: %s", e.what());
    }
    catch (...) {
      logger_->log_error("Unknown exception occurred while performing heartbeat.");
    }
  }

  checkTriggers();

  return utils::TaskRescheduleInfo::RetryIn(heart_beat_period_);
}

utils::TaskRescheduleInfo C2Agent::consume() {
  if (!responses.empty()) {
    const auto consume_success = responses.consume([this] (C2Payload&& payload) {
      extractPayload(std::move(payload));
    });
    if (!consume_success) {
      extractPayload(C2Payload{ Operation::HEARTBEAT });
    }
  }
  return utils::TaskRescheduleInfo::RetryIn(std::chrono::milliseconds(C2RESPONSE_POLL_MS));
}

std::optional<std::string> C2Agent::resolveFlowUrl(const std::string& url) const {
  if (utils::StringUtils::startsWith(url, "http")) {
    return url;
  }
  std::string base;
  if (configuration_->get(Configure::nifi_c2_flow_base_url, base)) {
    base = utils::StringUtils::trim(base);
    if (!utils::StringUtils::endsWith(base, "/")) {
      base += "/";
    }
    base += url;
    return base;
  } else if (configuration_->get(Configuration::nifi_c2_rest_url, "c2.rest.url", base)) {
    utils::URL base_url{utils::StringUtils::trim(base)};
    if (base_url.isValid()) {
      return base_url.hostPort() + "/c2/api/" + url;
    }
    logger_->log_error("Could not parse C2 REST URL '%s'", base);
    return std::nullopt;
  }
  return url;
}

std::optional<std::string> C2Agent::resolveUrl(const std::string& url) const {
  if (!utils::StringUtils::startsWith(url, "/")) {
    return url;
  }
  std::string base;
  if (!configuration_->get(Configuration::nifi_c2_rest_url, "c2.rest.url", base)) {
    logger_->log_error("Missing C2 REST URL");
    return std::nullopt;
  }
  utils::URL base_url{utils::StringUtils::trim(base)};
  if (base_url.isValid()) {
    return base_url.hostPort() + url;
  }
  logger_->log_error("Could not parse C2 REST URL '%s'", base);
  return std::nullopt;
}

std::optional<std::string> C2Agent::fetchFlow(const std::string& uri) const {
  if (!utils::StringUtils::startsWith(uri, "http") || protocol_.load() == nullptr) {
    // try to open the file
    auto content = filesystem_->read(uri);
    if (content) {
      return content;
    }
  }
  if (protocol_.load() == nullptr) {
    logger_->log_error("Couldn't open '%s' as file and we have no protocol to request the file from", uri);
    return {};
  }

  std::optional<std::string> resolved_url = resolveFlowUrl(uri);
  if (!resolved_url) {
    return std::nullopt;
  }

  C2Payload payload(Operation::TRANSFER, true);
  C2Payload &&response = protocol_.load()->consumePayload(resolved_url.value(), payload, RECEIVE, false);

  return response.getRawDataAsString();
}

bool C2Agent::handleConfigurationUpdate(const C2ContentResponse &resp) {
  auto url = resp.operation_arguments.find("location");

  std::string file_uri;
  std::string configuration_str;

  if (url != resp.operation_arguments.end()) {
    file_uri = url->second.to_string();
    std::optional<std::string> optional_configuration_str = fetchFlow(file_uri);
    if (!optional_configuration_str) {
      logger_->log_error("Couldn't load new flow configuration from: \"%s\"", file_uri);
      C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, true);
      response.setRawData("Error while applying flow. Couldn't load flow configuration.");
      enqueue_c2_response(std::move(response));
      return false;
    }
    configuration_str = optional_configuration_str.value();
  } else {
    logger_->log_debug("Did not have location within %s", resp.ident);
    auto update_text = resp.operation_arguments.find("configuration_data");
    if (update_text == resp.operation_arguments.end()) {
      logger_->log_error("Neither the config file location nor the data is provided");
      C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, true);
      response.setRawData("Error while applying flow. Neither the config file location nor the data is provided.");
      enqueue_c2_response(std::move(response));
      return false;
    }
    configuration_str = update_text->second.to_string();
  }

  bool should_persist = [&] {
    auto persist = resp.operation_arguments.find("persist");
    if (persist == resp.operation_arguments.end()) {
      return false;
    }
    return utils::StringUtils::equalsIgnoreCase(persist->second.to_string(), "true");
  }();

  int16_t err = {update_sink_->applyUpdate(file_uri, configuration_str, should_persist)};
  if (err != 0) {
    logger_->log_error("Flow configuration update failed with error code %" PRIi16, err);
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, true);
    response.setRawData("Error while applying flow. Likely missing processors");
    enqueue_c2_response(std::move(response));
    return false;
  }

  C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, true);
  enqueue_c2_response(std::move(response));

  if (should_persist) {
    // update the flow id
    configuration_->commitChanges();
  }

  return true;
}

static auto make_path(const std::string& str) {
  return std::filesystem::path(str);
}

static std::optional<std::string> validateFilePath(const std::filesystem::path& path) {
  if (path.empty()) {
    return "Empty file path";
  }
  if (!path.is_relative()) {
    return "File path must be a relative path '" + path.string() + "'";
  }
  if (!path.has_filename()) {
    return "Filename missing in output path '" + path.string() + "'";
  }
  if (path.filename() == "." || path.filename() == "..") {
    return "Invalid filename '" + path.filename().string() + "'";
  }
  for (const auto& segment : path) {
    if (segment == "..") {
      return "Accessing parent directory is forbidden in file path '" + path.string() + "'";
    }
  }
  return std::nullopt;
}

void C2Agent::handleAssetUpdate(const C2ContentResponse& resp) {
  auto send_error = [&] (std::string_view error) {
    logger_->log_error("%s", std::string(error));
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, true);
    response.setRawData(gsl::span<const char>(error).as_span<const std::byte>());
    enqueue_c2_response(std::move(response));
  };
  std::filesystem::path asset_dir = std::filesystem::path(configuration_->getHome()) / "asset";
  if (auto asset_dir_str = configuration_->get(Configuration::nifi_asset_directory)) {
    asset_dir = asset_dir_str.value();
  }

  // output file
  std::filesystem::path file_path;
  if (auto file_rel = resp.getArgument("file") | utils::map(make_path)) {
    if (auto error = validateFilePath(file_rel.value())) {
      send_error(error.value());
      return;
    }
    file_path = asset_dir / file_rel.value();
  } else {
    send_error("Couldn't find 'file' argument");
    return;
  }

  // source url
  std::string url;
  if (auto url_str = resp.getArgument("url")) {
    if (auto resolved_url = resolveUrl(*url_str)) {
      url = resolved_url.value();
    } else {
      send_error("Couldn't resolve url");
      return;
    }
  } else {
    send_error("Couldn't find 'url' argument");
    return;
  }

  // forceDownload
  bool force_download = false;
  if (auto force_download_str = resp.getArgument("forceDownload")) {
    if (utils::StringUtils::equalsIgnoreCase(force_download_str.value(), "true")) {
      force_download = true;
    } else if (utils::StringUtils::equalsIgnoreCase(force_download_str.value(), "false")) {
      force_download = false;
    } else {
      send_error("Argument 'forceDownload' must be either 'true' or 'false'");
      return;
    }
  }

  if (!force_download && std::filesystem::exists(file_path)) {
    logger_->log_info("File already exists");
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::NO_OPERATION, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  C2Payload file_response = protocol_.load()->consumePayload(url, C2Payload(Operation::TRANSFER, true), RECEIVE, false);

  if (file_response.getStatus().getState() != state::UpdateState::READ_COMPLETE) {
    send_error("Failed to fetch asset from '" + url + "'");
    return;
  }

  auto raw_data = std::move(file_response).moveRawData();
  // ensure directory exists for file
  if (utils::file::create_dir(file_path.parent_path().string()) != 0) {
    send_error("Failed to create directory '" + file_path.parent_path().string() + "'");
    return;
  }

  {
    std::ofstream file{file_path, std::ofstream::binary};
    file.write(reinterpret_cast<const char*>(raw_data.data()), raw_data.size());
  }

  C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, true);
  enqueue_c2_response(std::move(response));
}

void C2Agent::enqueue_c2_server_response(C2Payload &&resp) {
  logger_->log_trace("Server response: %s", [&] {return resp.str();});

  responses.enqueue(std::move(resp));
}

}  // namespace org::apache::nifi::minifi::c2
