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

#include "core/ProcessContext.h"
#include "core/StateManager.h"
#include "core/state/UpdateController.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileSystem.h"
#include "http/BaseHTTPClient.h"
#include "utils/file/PathUtils.h"
#include "utils/Environment.h"
#include "utils/Monitors.h"
#include "utils/StringUtils.h"
#include "io/ArchiveStream.h"
#include "io/StreamPipe.h"
#include "utils/Id.h"
#include "c2/C2Utils.h"
#include "c2/protocols/RESTSender.h"
#include "rapidjson/error/en.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::c2 {

C2Agent::C2Agent(std::shared_ptr<Configure> configuration,
                 std::weak_ptr<state::response::NodeReporter> node_reporter,
                 std::shared_ptr<utils::file::FileSystem> filesystem,
                 std::function<void()> request_restart,
                 utils::file::AssetManager* asset_manager)
    : heart_beat_period_(3s),
      max_c2_responses(5),
      configuration_(std::move(configuration)),
      node_reporter_(std::move(node_reporter)),
      filesystem_(std::move(filesystem)),
      thread_pool_(2, nullptr, "C2 threadpool"),
      request_restart_(std::move(request_restart)),
      last_run_(std::chrono::steady_clock::now()),
      asset_manager_(asset_manager) {
  if (!configuration_->getAgentClass()) {
    logger_->log_info("Agent class is not predefined");
  }

  // Set a persistent fallback agent id. This is needed so that the C2 server can identify the same agent after a restart, even if nifi.c2.agent.identifier is not specified.
  if (auto id = configuration_->get(Configuration::nifi_c2_agent_identifier_fallback)) {
    configuration_->setFallbackAgentIdentifier(*id);
  } else {
    const auto agent_id = utils::IdGenerator::getIdGenerator()->generate().to_string();
    configuration_->setFallbackAgentIdentifier(agent_id);
    configuration_->set(Configuration::nifi_c2_agent_identifier_fallback, agent_id, PropertyChangeLifetime::PERSISTENT);
  }
}

void C2Agent::initialize(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, state::StateMonitor* update_sink) {
  controller_ = controller;
  pause_handler_ = pause_handler;
  update_sink_ = update_sink;

  if (nullptr != controller_) {
    if (auto service = controller_->getControllerService(UPDATE_NAME)) {
      if (auto update_service = std::dynamic_pointer_cast<controllers::UpdatePolicyControllerService>(service)) {
        update_service_ = update_service;
      } else {
        logger_->log_warn("Found controller service with name '{}', but it is not an UpdatePolicyControllerService", c2::UPDATE_NAME);
      }
    }
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
    std::future<utils::TaskRescheduleInfo> future;
    thread_pool_.execute(utils::Worker{function, uuid.to_string()}, future);
  }
  controller_running_ = true;
  thread_pool_.start();
  logger_->log_info("C2 agent started");
}

void C2Agent::stop() {
  if (!controller_running_) {
    return;
  }
  controller_running_ = false;
  for (const auto& id : task_ids_) {
    thread_pool_.stopTasks(id.to_string());
  }
  thread_pool_.shutdown();
  logger_->log_info("C2 agent stopped");
}

void C2Agent::checkTriggers() {
  logger_->log_debug("Checking {} triggers", triggers_.size());
  for (const auto &trigger : triggers_) {
    if (trigger->triggered()) {
      /**
       * Action was triggered, so extract it.
       */
      C2Payload &&triggerAction = trigger->getAction();
      logger_->log_trace("{} action triggered", trigger->getName());
      // handle the response the same way. This means that
      // acknowledgements will be sent to the c2 server for every trigger action.
      // this is expected
      extractPayload(triggerAction);
      // call reset if the trigger supports this activity
      trigger->reset();
    } else {
      logger_->log_trace("{} action not triggered", trigger->getName());
    }
  }
}
void C2Agent::configure(const std::shared_ptr<Configure> &configure, bool reconfigure) {
  if (!reconfigure) {
    protocol_ = std::make_unique<RESTSender>("RESTSender");
    protocol_->initialize(controller_, configuration_);
  } else {
    protocol_->update(configure);
  }

  std::string heartbeat_period;
  if (configure->get(Configuration::nifi_c2_agent_heartbeat_period, "c2.agent.heartbeat.period", heartbeat_period)) {
    try {
      if (auto heartbeat_period_ms = utils::timeutils::StringToDuration<std::chrono::milliseconds>(heartbeat_period)) {
        heart_beat_period_ = *heartbeat_period_ms;
      } else {
        heart_beat_period_ = std::chrono::milliseconds(std::stoi(heartbeat_period));
      }
    } catch (const std::invalid_argument &) {
      logger_->log_error("Invalid heartbeat period: {}", heartbeat_period);
      heart_beat_period_ = 3s;
    }
  } else {
    if (!reconfigure)
      heart_beat_period_ = 3s;
  }
  logger_->log_debug("Using {} as the heartbeat period", heart_beat_period_);

  std::string heartbeat_reporters;
  if (configure->get(Configuration::nifi_c2_agent_heartbeat_reporter_classes, "c2.agent.heartbeat.reporter.classes", heartbeat_reporters)) {
    std::vector<std::string> reporters = utils::string::splitAndTrim(heartbeat_reporters, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& reporter : reporters) {
      auto heartbeat_reporter_obj = core::ClassLoader::getDefaultClassLoader().instantiate<HeartbeatReporter>(reporter, reporter);
      if (heartbeat_reporter_obj == nullptr) {
        logger_->log_error("Could not instantiate {}", reporter);
      } else {
        heartbeat_reporter_obj->initialize(controller_, update_sink_, configuration_);
        heartbeat_protocols_.push_back(std::move(heartbeat_reporter_obj));
      }
    }
  }

  std::string trigger_classes;
  if (configure->get(Configuration::nifi_c2_agent_trigger_classes, "c2.agent.trigger.classes", trigger_classes)) {
    std::vector<std::string> triggers = utils::string::splitAndTrim(trigger_classes, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& trigger : triggers) {
      auto trigger_obj = core::ClassLoader::getDefaultClassLoader().instantiate<C2Trigger>(trigger, trigger);
      if (trigger_obj == nullptr) {
        logger_->log_error("Could not instantiate {}", trigger);
      } else {
        trigger_obj->initialize(configuration_);
        triggers_.push_back(std::move(trigger_obj));
      }
    }
  }
}

void C2Agent::performHeartBeat() {
  C2Payload payload(Operation::heartbeat);
  logger_->log_trace("Performing heartbeat");
  std::vector<state::response::NodeReporter::ReportedNode> metrics;
  if (auto node_reporter = node_reporter_.lock()) {
    if (!manifest_sent_) {
      // include agent manifest for the first heartbeat
      metrics = node_reporter->getHeartbeatNodes(true);
      manifest_sent_ = true;
    } else {
      metrics = node_reporter->getHeartbeatNodes(false);
    }
  }

  payload.reservePayloads(metrics.size());
  for (const auto& metric : metrics) {
    C2Payload child_metric_payload(Operation::heartbeat);
    child_metric_payload.setLabel(metric.name);
    child_metric_payload.setContainer(metric.is_array);
    serializeMetrics(child_metric_payload, metric.name, metric.serialized_nodes, metric.is_array);
    payload.addPayload(std::move(child_metric_payload));
  }
  C2Payload response = protocol_->consumePayload(payload);

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
    if (metric.keep_empty || !metric.children.empty()) {
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
      response.operation_arguments[metric.name] = C2Value{metric.value};
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
      logger_->log_error("Received unknown event ({}) from protocol", static_cast<int>(resp.getStatus().getState()));
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
  switch (resp.op) {
    case Operation::clear:
      handle_clear(resp);
      break;
    case Operation::update:
      handle_update(resp);
      break;
    case Operation::describe:
      handle_describe(resp);
      break;
    case Operation::restart: {
      update_sink_->stop();
      C2Payload response(Operation::acknowledge, resp.ident, true);
      protocol_->consumePayload(response);
      restart_needed_ = true;
    }
      break;
    case Operation::start:
    case Operation::stop: {
      if (resp.name == "C2" || resp.name == "c2") {
        (void)raise(SIGTERM);
      }

      // stop all referenced components.
      update_sink_->executeOnComponent(resp.name, [this, &resp] (state::StateController& component) {
        logger_->log_debug("Stopping component {}", resp.name);
        if (resp.op == Operation::stop) {
          component.stop();
        } else {
          component.start();
        }
      });

      if (resp.ident.length() > 0) {
        C2Payload response(Operation::acknowledge, resp.ident, true);
        enqueue_c2_response(std::move(response));
      }
    }
      //
      break;
    case Operation::pause:
      if (pause_handler_ != nullptr) {
        pause_handler_->pause();
      } else {
        logger_->log_warn("Pause functionality is not supported!");
      }
      break;
    case Operation::resume:
      if (pause_handler_ != nullptr) {
        pause_handler_->resume();
      } else {
        logger_->log_warn("Resume functionality is not supported!");
      }
      break;
    case Operation::transfer: {
      try {
        handle_transfer(resp);
        C2Payload response(Operation::acknowledge, resp.ident, true);
        enqueue_c2_response(std::move(response));
      } catch (const std::runtime_error& error) {
        C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
        response.setRawData(error.what());
        enqueue_c2_response(std::move(response));
      }
      break;
    }
    case Operation::sync:
      handle_sync(resp);
      break;
    default:
      break;
      // do nothing
  }
}

C2Payload C2Agent::prepareConfigurationOptions(const C2ContentResponse &resp) const {
    auto unsanitized_keys = configuration_->getConfiguredKeys();
    std::vector<std::string> keys;
    std::copy_if(unsanitized_keys.begin(), unsanitized_keys.end(), std::back_inserter(keys),
            [](const std::string& key) {return key.find("pass") == std::string::npos;});

    C2Payload response(Operation::acknowledge, resp.ident, true);
    C2Payload options(Operation::acknowledge);
    options.setLabel("configuration_options");
    std::string value;
    for (const auto& key : keys) {
      if (configuration_->get(key, value)) {
        C2ContentResponse option(Operation::acknowledge);
        option.name = key;
        option.operation_arguments[key] = C2Value{value};
        options.addContent(std::move(option));
      }
    }
    response.addPayload(std::move(options));
    return response;
}

void C2Agent::handle_clear(const C2ContentResponse &resp) {
  ClearOperand operand = ClearOperand::connection;
  try {
    operand = utils::enumCast<ClearOperand>(resp.name, true);
  } catch(const std::runtime_error&) {
    logger_->log_debug("Clearing unknown {}", resp.name);
    return;
  }

  switch (operand) {
    case ClearOperand::connection: {
      for (const auto& connection : resp.operation_arguments) {
        logger_->log_debug("Clearing connection {}", connection.second.to_string());
        update_sink_->clearConnection(connection.second.to_string());
      }
      break;
    }
    case ClearOperand::repositories: {
      update_sink_->drainRepositories();
      break;
    }
    case ClearOperand::corecomponentstate: {
      for (const auto& corecomponent : resp.operation_arguments) {
        auto state_storage = core::ProcessContextImpl::getStateStorage(logger_, controller_, configuration_);
        if (state_storage != nullptr) {
          update_sink_->executeOnComponent(corecomponent.second.to_string(), [this, &state_storage] (state::StateController& component) {
            logger_->log_debug("Clearing state for component {}", component.getComponentName());
            auto state_manager = state_storage->getStateManager(component.getComponentUUID());
            if (state_manager != nullptr) {
              component.stop();
              state_manager->clear();
              state_manager->persist();
              component.start();
            } else {
              logger_->log_warn("Failed to get StateManager for component {}", component.getComponentUUID().to_string());
            }
          });
        } else {
          logger_->log_error("Failed to get StateStorage");
        }
      }
      break;
    }
    default:
      logger_->log_error("Unknown clear operand {}", resp.name);
  }

  C2Payload response(Operation::acknowledge, resp.ident, true);
  enqueue_c2_response(std::move(response));
}

/**
 * Descriptions are special types of requests that require information
 * to be put into the acknowledgement
 */
void C2Agent::handle_describe(const C2ContentResponse &resp) {
  DescribeOperand operand = DescribeOperand::metrics;
  try {
    operand = utils::enumCast<DescribeOperand>(resp.name, true);
  } catch(const std::runtime_error&) {
    C2Payload response(Operation::acknowledge, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  switch (operand) {
    case DescribeOperand::metrics: {
      C2Payload response(Operation::acknowledge, resp.ident, true);
      auto iter = resp.operation_arguments.find("metricsClass");
      std::string metricsClass;
      if (iter != resp.operation_arguments.end()) {
        metricsClass = iter->second.to_string();
      }
      std::optional<state::response::NodeReporter::ReportedNode> metricsNode;
      if (auto node_reporter = node_reporter_.lock()) {
        metricsNode = node_reporter->getMetricsNode(metricsClass);
      }
      C2Payload metrics(Operation::acknowledge);
      metricsClass.empty() ? metrics.setLabel("metrics") : metrics.setLabel(metricsClass);
      if (metricsNode) {
        serializeMetrics(metrics, metricsNode->name, metricsNode->serialized_nodes, metricsNode->is_array);
      }
      response.addPayload(std::move(metrics));
      enqueue_c2_response(std::move(response));
      break;
    }
    case DescribeOperand::configuration: {
      auto configOptions = prepareConfigurationOptions(resp);
      enqueue_c2_response(std::move(configOptions));
      break;
    }
    case DescribeOperand::manifest: {
      C2Payload response(prepareConfigurationOptions(resp));
      C2Payload agentInfo(Operation::acknowledge, resp.ident, true);
      agentInfo.setLabel("agentInfo");

      if (auto node_reporter = node_reporter_.lock()) {
        auto manifest = node_reporter->getAgentManifest();
        serializeMetrics(agentInfo, manifest.name, manifest.serialized_nodes);
      }
      response.addPayload(std::move(agentInfo));
      enqueue_c2_response(std::move(response));
      break;
    }
    case DescribeOperand::jstack: {
      if (update_sink_->isRunning()) {
        const std::vector<BackTrace> traces = update_sink_->getTraces();
        for (const auto &trace : traces) {
          for (const auto & line : trace.getTraces()) {
            logger_->log_trace("{} -- {}", trace.getName(), line);
          }
        }
        auto keys = configuration_->getConfiguredKeys();
        C2Payload response(Operation::acknowledge, resp.ident, true);
        for (const auto &trace : traces) {
          C2Payload options(Operation::acknowledge, resp.ident, true);
          options.setLabel(trace.getName());
          std::string value;
          for (const auto &line : trace.getTraces()) {
            C2ContentResponse option(Operation::acknowledge);
            option.name = line;
            option.operation_arguments[line] = C2Value{line};
            options.addContent(std::move(option));
          }
          response.addPayload(std::move(options));
        }
        enqueue_c2_response(std::move(response));
      }
      break;
    }
    case DescribeOperand::corecomponentstate: {
      C2Payload response(Operation::acknowledge, resp.ident, true);
      response.setLabel("corecomponentstate");
      C2Payload states(Operation::acknowledge, resp.ident, true);
      states.setLabel("corecomponentstate");
      auto state_storage = core::ProcessContextImpl::getStateStorage(logger_, controller_, configuration_);
      if (state_storage != nullptr) {
        auto core_component_states = state_storage->getAllStates();
        for (const auto& core_component_state : core_component_states) {
          C2Payload state(Operation::acknowledge, resp.ident, true);
          state.setLabel(core_component_state.first.to_string());
          for (const auto& kv : core_component_state.second) {
            C2ContentResponse entry(Operation::acknowledge);
            entry.name = kv.first;
            entry.operation_arguments[kv.first] = C2Value{kv.second};
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
  UpdateOperand operand = UpdateOperand::configuration;
  try {
    operand = utils::enumCast<UpdateOperand>(resp.name, true);
  } catch(const std::runtime_error&) {
    C2Payload response(Operation::acknowledge, state::UpdateState::NOT_APPLIED, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  switch (operand) {
    case UpdateOperand::configuration: {
      handleConfigurationUpdate(resp);
      break;
    }
    case UpdateOperand::properties: {
      handlePropertyUpdate(resp);
      break;
    }
    case UpdateOperand::asset: {
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

  for (const auto& [name, value] : resp.operation_arguments) {
    if (auto* json_val = value.json()) {
      if (json_val->IsObject() && json_val->HasMember("value")) {
        PropertyChangeLifetime lifetime = PropertyChangeLifetime::PERSISTENT;
        if (json_val->HasMember("persist")) {
          lifetime = (*json_val)["persist"].GetBool() ? PropertyChangeLifetime::PERSISTENT : PropertyChangeLifetime::TRANSIENT;
        }
        std::string property_value{(*json_val)["value"].GetString(), (*json_val)["value"].GetStringLength()};
        changeUpdateState(update_property(name, property_value, lifetime));
        continue;
      }
    }
    changeUpdateState(update_property(name, value.to_string(), PropertyChangeLifetime::PERSISTENT));
  }
  // apply changes and persist properties requested to be persisted
  const bool propertyWasUpdated = result == state::UpdateState::FULLY_APPLIED || result == state::UpdateState::PARTIALLY_APPLIED;
  if (propertyWasUpdated && !configuration_->commitChanges()) {
    result = state::UpdateState::PARTIALLY_APPLIED;
  }
  C2Payload response(Operation::acknowledge, result, resp.ident, true);
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
  static constexpr const char* MANIFEST_FILE_NAME = "manifest.json";
  auto manifest_stream = std::make_unique<io::BufferStream>();
  if (auto node_reporter = node_reporter_.lock()) {
    auto reported_manifest = node_reporter->getAgentManifest();
    std::string manifest_str = state::response::SerializedResponseNode{
      .name = std::move(reported_manifest.name),
      .array = reported_manifest.is_array,
      .children = std::move(reported_manifest.serialized_nodes)
    }.to_pretty_string();
    manifest_stream->write(as_bytes(std::span(manifest_str)));
  }
  files[MANIFEST_FILE_NAME] = std::move(manifest_stream);

  auto bundle = createDebugBundleArchive(files);
  if (!bundle) {
    throw C2DebugBundleError(bundle.error());
  }

  C2Payload file(Operation::transfer, true);
  file.setLabel("debug.tar.gz");
  file.setRawData(bundle.value()->moveBuffer());
  C2Payload payload(Operation::transfer, false);
  payload.addPayload(std::move(file));
  return payload;
}

void C2Agent::handle_transfer(const C2ContentResponse &resp) {
  TransferOperand operand = TransferOperand::debug;
  try {
    operand = utils::enumCast<TransferOperand>(resp.name, true);
  } catch(const std::runtime_error&) {
    throw C2TransferError("Unknown operand '" + resp.name + "'");
  }

  switch (operand) {
    case TransferOperand::debug: {
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
      C2Payload &&response = protocol_->consumePayload(url.value(), bundle, TRANSMIT, false);
      if (response.getStatus().getState() == state::UpdateState::READ_ERROR) {
        throw C2DebugBundleError("Error while uploading");
      }
      break;
    }
  }
}

void C2Agent::handle_sync(const org::apache::nifi::minifi::c2::C2ContentResponse &resp) {
  logger_->log_info("Requested resource synchronization");
  auto send_error = [&] (std::string_view error) {
    logger_->log_error("{}", error);
    C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
    response.setRawData(as_bytes(std::span(error.begin(), error.end())));
    enqueue_c2_response(std::move(response));
  };

  if (!asset_manager_) {
    send_error("Internal error: no asset manager");
    return;
  }

  SyncOperand operand = SyncOperand::resource;
  try {
    operand = utils::enumCast<SyncOperand>(resp.name, true);
  } catch(const std::runtime_error&) {
    send_error("Unknown operand '" + resp.name + "'");
    return;
  }

  gsl_Assert(operand == SyncOperand::resource);

  utils::file::AssetLayout asset_layout;

  auto state_it = resp.operation_arguments.find("globalHash");
  if (state_it == resp.operation_arguments.end()) {
    send_error("Malformed request, missing 'globalHash' argument");
    return;
  }

  const rapidjson::Document* state_doc = state_it->second.json();
  if (!state_doc) {
    send_error("Argument 'globalHash' is malformed");
    return;
  }

  if (!state_doc->IsObject()) {
    send_error("Malformed request, 'globalHash' is not an object");
    return;
  }

  if (!state_doc->HasMember("digest")) {
    send_error("Malformed request, 'globalHash' has no member 'digest'");
    return;
  }
  if (!(*state_doc)["digest"].IsString()) {
    send_error("Malformed request, 'globalHash.digest' is not a string");
    return;
  }

  asset_layout.digest = std::string{(*state_doc)["digest"].GetString(), (*state_doc)["digest"].GetStringLength()};

  auto resource_list_it = resp.operation_arguments.find("resourceList");
  if (resource_list_it == resp.operation_arguments.end()) {
    send_error("Malformed request, missing 'resourceList' argument");
    return;
  }

  const rapidjson::Document* resource_list = resource_list_it->second.json();
  if (!resource_list) {
    send_error("Argument 'resourceList' is malformed");
    return;
  }
  if (!resource_list->IsArray()) {
    send_error("Malformed request, 'resourceList' is not an array");
    return;
  }

  for (rapidjson::SizeType resource_idx = 0; resource_idx < resource_list->Size(); ++resource_idx) {
    auto& resource = resource_list->GetArray()[resource_idx];
    if (!resource.IsObject()) {
      send_error(fmt::format("Malformed request, 'resourceList[{}]' is not an object", resource_idx));
      return;
    }
    auto get_member_str = [&] (const char* key) -> nonstd::expected<std::string_view, std::string> {
      if (!resource.HasMember(key)) {
        return nonstd::make_unexpected(fmt::format("Malformed request, 'resourceList[{}]' has no member '{}'", resource_idx, key));
      }
      if (!resource[key].IsString()) {
        return nonstd::make_unexpected(fmt::format("Malformed request, 'resourceList[{}].{}' is not a string", resource_idx, key));
      }
      return std::string_view{resource[key].GetString(), resource[key].GetStringLength()};
    };
    auto id = get_member_str("resourceId");
    if (!id) {
      send_error(id.error());
      return;
    }
    auto name = get_member_str("resourceName");
    if (!name) {
      send_error(name.error());
      return;
    }
    auto type = get_member_str("resourceType");
    if (!type) {
      send_error(type.error());
      return;
    }
    if (type.value() != "ASSET") {
      logger_->log_info("Resource (id = '{}', name = '{}') with type '{}' is not yet supported", id.value(), name.value(), type.value());
      continue;
    }
    auto path = get_member_str("resourcePath");
    if (!path) {
      send_error(path.error());
      return;
    }
    auto url = get_member_str("url");
    if (!url) {
      send_error(url.error());
      return;
    }

    auto full_path = std::filesystem::path{path.value()} / name.value();  // NOLINT(whitespace/braces)

    auto path_valid = utils::file::validateRelativeFilePath(full_path);
    if (!path_valid) {
      send_error(path_valid.error());
      return;
    }

    asset_layout.assets.insert(utils::file::AssetDescription{
        .id = std::string{id.value()},
        .path = full_path,
        .url = std::string{url.value()}
    });
  }

  auto fetch = [&] (std::string_view url) -> nonstd::expected<std::vector<std::byte>, std::string> {
    auto resolved_url = resolveUrl(std::string{url});
    if (!resolved_url) {
      return nonstd::make_unexpected("Couldn't resolve url");
    }
    C2Payload file_response = protocol_->fetch(resolved_url.value());

    if (file_response.getStatus().getState() != state::UpdateState::READ_COMPLETE) {
      return nonstd::make_unexpected("Failed to fetch file from " + resolved_url.value());
    }

    return std::move(file_response).moveRawData();
  };

  auto result = asset_manager_->sync(asset_layout, fetch);
  if (!result) {
    send_error(result.error());
    return;
  }

  C2Payload response(Operation::acknowledge, state::UpdateState::FULLY_APPLIED, resp.ident, true);
  enqueue_c2_response(std::move(response));
}

utils::TaskRescheduleInfo C2Agent::produce() {
  // place priority on messages to send to the c2 server
  if (protocol_ != nullptr) {
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
            C2Payload response = protocol_->consumePayload(payload);
            enqueue_c2_server_response(std::move(response));
          }
          catch(const std::exception &e) {
            logger_->log_error("Exception occurred while consuming payload. error: {}", e.what());
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
      logger_->log_error("Exception occurred while performing heartbeat. type: {}, what: {}", typeid(e).name(), e.what());
    }
    catch (...) {
      logger_->log_error("Unknown exception occurred while performing heartbeat, type: {}", getCurrentExceptionTypeName());
    }
  }

  checkTriggers();

  return utils::TaskRescheduleInfo::RetryIn(heart_beat_period_);
}

utils::TaskRescheduleInfo C2Agent::consume() {
  if (!responses.empty()) {
    const auto consume_success = responses.consume([this] (C2Payload&& payload) {
      extractPayload(payload);
    });
    if (!consume_success) {
      extractPayload(C2Payload{ Operation::heartbeat });
    }
  }
  return utils::TaskRescheduleInfo::RetryIn(std::chrono::milliseconds(C2RESPONSE_POLL_MS));
}

std::optional<std::string> C2Agent::resolveFlowUrl(const std::string& url) const {
  if (utils::string::startsWith(url, "http")) {
    return url;
  }
  std::string base;
  if (configuration_->get(Configure::nifi_c2_flow_base_url, base)) {
    base = utils::string::trim(base);
    if (!utils::string::endsWith(base, "/")) {
      base += "/";
    }
    base += url;
    return base;
  } else if (configuration_->get(Configuration::nifi_c2_rest_url, "c2.rest.url", base)) {
    http::URL base_url{utils::string::trim(base)};
    if (base_url.isValid()) {
      return base_url.hostPort() + "/c2/api/" + url;
    }
    logger_->log_error("Could not parse C2 REST URL '{}'", base);
    return std::nullopt;
  }
  return url;
}

std::optional<std::string> C2Agent::resolveUrl(const std::string& url) const {
  if (!utils::string::startsWith(url, "/")) {
    return url;
  }
  std::string base;
  if (configuration_->get(Configuration::nifi_c2_rest_path_base, base)) {
    return base + url;
  }
  if (!configuration_->get(Configuration::nifi_c2_rest_url, "c2.rest.url", base)) {
    logger_->log_error("Missing C2 REST URL");
    return std::nullopt;
  }
  http::URL base_url{utils::string::trim(base)};
  if (base_url.isValid()) {
    return base_url.hostPort() + url;
  }
  logger_->log_error("Could not parse C2 REST URL '{}'", base);
  return std::nullopt;
}

std::optional<std::string> C2Agent::fetchFlow(const std::string& uri) const {
  if (!utils::string::startsWith(uri, "http") || protocol_ == nullptr) {
    // try to open the file
    auto content = filesystem_->read(uri);
    if (content) {
      return content;
    }
  }
  if (protocol_ == nullptr) {
    logger_->log_error("Couldn't open '{}' as file and we have no protocol to request the file from", uri);
    return {};
  }

  std::optional<std::string> resolved_url = resolveFlowUrl(uri);
  if (!resolved_url) {
    return std::nullopt;
  }

  C2Payload response = protocol_->fetch(resolved_url.value(), update_sink_->getSupportedConfigurationFormats());

  return response.getRawDataAsString();
}

std::optional<std::string> C2Agent::getFlowIdFromConfigUpdate(const C2ContentResponse &resp) {
  auto flow_id = resp.operation_arguments.find("flowId");
  return flow_id == resp.operation_arguments.end() ? std::nullopt : std::make_optional(flow_id->second.to_string());
}

bool C2Agent::handleConfigurationUpdate(const C2ContentResponse &resp) {
  auto url = resp.operation_arguments.find("location");

  std::string file_uri;
  std::string configuration_str;

  if (url != resp.operation_arguments.end()) {
    file_uri = url->second.to_string();
    std::optional<std::string> optional_configuration_str = fetchFlow(file_uri);
    if (!optional_configuration_str) {
      logger_->log_error("Couldn't load new flow configuration from: \"{}\"", file_uri);
      C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
      response.setRawData("Error while applying flow. Couldn't load flow configuration.");
      enqueue_c2_response(std::move(response));
      return false;
    }
    configuration_str = optional_configuration_str.value();
  } else {
    logger_->log_debug("Did not have location within {}", resp.ident);
    auto update_text = resp.operation_arguments.find("configuration_data");
    if (update_text == resp.operation_arguments.end()) {
      logger_->log_error("Neither the config file location nor the data is provided");
      C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
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
    return utils::string::equalsIgnoreCase(persist->second.to_string(), "true");
  }();

  int16_t err = {update_sink_->applyUpdate(file_uri, configuration_str, should_persist, getFlowIdFromConfigUpdate(resp))};
  if (err != 0) {
    logger_->log_error("Flow configuration update failed with error code {}", err);
    C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
    response.setRawData("Error while applying flow. Likely missing processors");
    enqueue_c2_response(std::move(response));
    return false;
  }

  C2Payload response(Operation::acknowledge, state::UpdateState::FULLY_APPLIED, resp.ident, true);
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

void C2Agent::handleAssetUpdate(const C2ContentResponse& resp) {
  auto send_error = [&] (std::string_view error) {
    logger_->log_error("{}", error);
    C2Payload response(Operation::acknowledge, state::UpdateState::SET_ERROR, resp.ident, true);
    response.setRawData(as_bytes(std::span(error.begin(), error.end())));
    enqueue_c2_response(std::move(response));
  };

  // output file
  std::filesystem::path file_path;
  if (auto file_rel = resp.getStringArgument("file") | utils::transform(make_path)) {
    auto result = utils::file::validateRelativeFilePath(file_rel.value());
    if (!result) {
      send_error(result.error());
      return;
    }
    file_path = asset_manager_->getRoot() / file_rel.value();
  } else {
    send_error("Couldn't find 'file' argument");
    return;
  }

  // source url
  std::string url;
  if (auto url_str = resp.getStringArgument("url")) {
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
  if (auto force_download_str = resp.getStringArgument("forceDownload")) {
    if (utils::string::equalsIgnoreCase(force_download_str.value(), "true")) {
      force_download = true;
    } else if (utils::string::equalsIgnoreCase(force_download_str.value(), "false")) {
      force_download = false;
    } else {
      send_error("Argument 'forceDownload' must be either 'true' or 'false'");
      return;
    }
  }

  if (!force_download && std::filesystem::exists(file_path)) {
    logger_->log_info("File already exists");
    C2Payload response(Operation::acknowledge, state::UpdateState::NO_OPERATION, resp.ident, true);
    enqueue_c2_response(std::move(response));
    return;
  }

  C2Payload file_response = protocol_->fetch(url);

  if (file_response.getStatus().getState() != state::UpdateState::READ_COMPLETE) {
    send_error("Failed to fetch asset from '" + url + "'");
    return;
  }

  auto raw_data = std::move(file_response).moveRawData();
  // ensure directory exists for file
  if (utils::file::create_dir(file_path.parent_path()) != 0) {
    send_error("Failed to create directory '" + file_path.parent_path().string() + "'");
    return;
  }

  {
    std::ofstream file{file_path, std::ofstream::binary};
    file.write(reinterpret_cast<const char*>(raw_data.data()), gsl::narrow<std::streamsize>(raw_data.size()));
  }

  C2Payload response(Operation::acknowledge, state::UpdateState::FULLY_APPLIED, resp.ident, true);
  enqueue_c2_response(std::move(response));
}

void C2Agent::enqueue_c2_server_response(C2Payload &&resp) {
  logger_->log_trace("Server response: {}", [&] { return resp.str(); });

  responses.enqueue(std::move(resp));
}

}  // namespace org::apache::nifi::minifi::c2
