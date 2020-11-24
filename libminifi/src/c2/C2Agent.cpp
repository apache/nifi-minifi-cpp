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

#include <cstdio>
#include <csignal>
#include <utility>
#include <limits>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <cinttypes>

#include "c2/ControllerSocketProtocol.h"
#include "core/ProcessContext.h"
#include "core/CoreComponentState.h"
#include "core/state/UpdateController.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/DiffUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileManager.h"
#include "utils/file/FileSystem.h"
#include "utils/GeneralUtils.h"
#include "utils/HTTPClient.h"
#include "utils/Environment.h"
#include "utils/Monitors.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2Agent::C2Agent(core::controller::ControllerServiceProvider* controller,
                 const std::shared_ptr<state::StateMonitor> &updateSink,
                 const std::shared_ptr<Configure> &configuration,
                 const std::shared_ptr<utils::file::FileSystem>& filesystem)
    : heart_beat_period_(3000),
      max_c2_responses(5),
      update_sink_(updateSink),
      update_service_(nullptr),
      controller_(controller),
      configuration_(configuration),
      filesystem_(filesystem),
      protocol_(nullptr),
      logger_(logging::LoggerFactory<C2Agent>::getLogger()),
      thread_pool_(2, false, nullptr, "C2 threadpool") {
  allow_updates_ = true;

  manifest_sent_ = false;

  running_c2_configuration = std::make_shared<Configure>();

  last_run_ = std::chrono::steady_clock::now();

  if (nullptr != controller_) {
    update_service_ = std::static_pointer_cast<controllers::UpdatePolicyControllerService>(controller_->getControllerService(UPDATE_NAME));
  }

  if (update_service_ == nullptr) {
    // create a stubbed service for updating the flow identifier
  }

  configure(configuration, false);

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
    auto monitor = utils::make_unique<utils::ComplexMonitor>();
    utils::Worker<utils::TaskRescheduleInfo> functor(function, uuid.to_string(), std::move(monitor));
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
    if (!configure->get("nifi.c2.agent.protocol.class", "c2.agent.protocol.class", clazz)) {
      clazz = "CoapProtocol";
    }
    logger_->log_info("Class is %s", clazz);

    auto protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw(clazz, clazz);
    if (protocol == nullptr) {
      logger_->log_warn("Class %s not found", clazz);
      protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw("CoapProtocol", "CoapProtocol");
      if (!protocol) {
        const char* errmsg = "Attempted to load CoapProtocol. To enable C2, please specify an active protocol for this agent.";
        logger_->log_error(errmsg);
        throw minifi::Exception{ minifi::GENERAL_EXCEPTION, errmsg };
      }

      logger_->log_info("Class is CoapProtocol");
    }

    // Since !reconfigure, the call comes from the ctor and protocol_ is null, therefore no delete is necessary
    protocol_.exchange(dynamic_cast<C2Protocol *>(protocol));

    protocol_.load()->initialize(controller_, configuration_);
  } else {
    protocol_.load()->update(configure);
  }

  if (configure->get("nifi.c2.agent.heartbeat.period", "c2.agent.heartbeat.period", heartbeat_period)) {
    core::TimeUnit unit;

    try {
      int64_t schedulingPeriod = 0;
      if (core::Property::StringToTime(heartbeat_period, schedulingPeriod, unit) && core::Property::ConvertTimeUnitToMS(schedulingPeriod, unit, schedulingPeriod)) {
        heart_beat_period_ = schedulingPeriod;
        logger_->log_debug("Using %u ms as the heartbeat period", heart_beat_period_);
      } else {
        heart_beat_period_ = std::stoi(heartbeat_period);
      }
    } catch (const std::invalid_argument &) {
      heart_beat_period_ = 3000;
    }
  } else {
    if (!reconfigure)
      heart_beat_period_ = 3000;
  }

  std::string update_settings;
  if (configure->get("nifi.c2.agent.update.allow", "c2.agent.update.allow", update_settings) && utils::StringUtils::StringToBool(update_settings, allow_updates_)) {
    // allow the agent to be updated. we then need to get an update command to execute after
  }

  if (allow_updates_) {
    if (!configure->get("nifi.c2.agent.update.command", "c2.agent.update.command", update_command_)) {
      std::string cwd = utils::Environment::getCurrentWorkingDirectory();
      if (cwd.empty()) {
        logger_->log_error("Could not set the update command because the working directory could not be determined");
      } else {
        update_command_ = cwd + "/minifi.sh update";
      }
    }

    if (!configure->get("nifi.c2.agent.update.temp.location", "c2.agent.update.temp.location", update_location_)) {
      std::string cwd = utils::Environment::getCurrentWorkingDirectory();
      if (cwd.empty()) {
        logger_->log_error("Could not set the update location because the working directory could not be determined");
      } else {
        update_location_ = cwd + "/minifi.update";
      }
    }

    // if not defined we won't be able to update
    configure->get("nifi.c2.agent.bin.location", "c2.agent.bin.location", bin_location_);
  }
  std::string heartbeat_reporters;
  if (configure->get("nifi.c2.agent.heartbeat.reporter.classes", "c2.agent.heartbeat.reporter.classes", heartbeat_reporters)) {
    std::vector<std::string> reporters = utils::StringUtils::split(heartbeat_reporters, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& reporter : reporters) {
      auto heartbeat_reporter_obj = core::ClassLoader::getDefaultClassLoader().instantiate(reporter, reporter);
      if (heartbeat_reporter_obj == nullptr) {
        logger_->log_debug("Could not instantiate %s", reporter);
      } else {
        std::shared_ptr<HeartBeatReporter> shp_reporter = std::static_pointer_cast<HeartBeatReporter>(heartbeat_reporter_obj);
        shp_reporter->initialize(controller_, update_sink_, configuration_);
        heartbeat_protocols_.push_back(shp_reporter);
      }
    }
  }

  std::string trigger_classes;
  if (configure->get("nifi.c2.agent.trigger.classes", "c2.agent.trigger.classes", trigger_classes)) {
    std::vector<std::string> triggers = utils::StringUtils::split(trigger_classes, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (const auto& trigger : triggers) {
      auto trigger_obj = core::ClassLoader::getDefaultClassLoader().instantiate(trigger, trigger);
      if (trigger_obj == nullptr) {
        logger_->log_debug("Could not instantiate %s", trigger);
      } else {
        std::shared_ptr<C2Trigger> trg_impl = std::static_pointer_cast<C2Trigger>(trigger_obj);
        trg_impl->initialize(configuration_);
        triggers_.push_back(trg_impl);
      }
    }
  }

  auto base_reporter = "ControllerSocketProtocol";
  auto heartbeat_reporter_obj = core::ClassLoader::getDefaultClassLoader().instantiate(base_reporter, base_reporter);
  if (heartbeat_reporter_obj == nullptr) {
    logger_->log_debug("Could not instantiate %s", base_reporter);
  } else {
    std::shared_ptr<HeartBeatReporter> shp_reporter = std::static_pointer_cast<HeartBeatReporter>(heartbeat_reporter_obj);
    shp_reporter->initialize(controller_, update_sink_, configuration_);
    heartbeat_protocols_.push_back(shp_reporter);
  }
}

void C2Agent::performHeartBeat() {
  C2Payload payload(Operation::HEARTBEAT);
  logger_->log_trace("Performing heartbeat");
  std::shared_ptr<state::response::NodeReporter> reporter = std::dynamic_pointer_cast<state::response::NodeReporter>(update_sink_);
  std::vector<std::shared_ptr<state::response::ResponseNode>> metrics;
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
      child_metric_payload.setLabel(metric->getName());
      child_metric_payload.setContainer(metric->isArray());
      serializeMetrics(child_metric_payload, metric->getName(), metric->serialize(), metric->isArray());
      payload.addPayload(std::move(child_metric_payload));
    }
  }
  C2Payload && response = protocol_.load()->consumePayload(payload);

  enqueue_c2_server_response(std::move(response));

  std::lock_guard<std::mutex> lock(heartbeat_mutex);

  for (auto reporter : heartbeat_protocols_) {
    reporter->heartbeat(payload);
  }
}

void C2Agent::serializeMetrics(C2Payload &metric_payload, const std::string &name, const std::vector<state::response::SerializedResponseNode> &metrics, bool is_container, bool is_collapsible) {
  const auto payloads = std::count_if(begin(metrics), end(metrics), [](const state::response::SerializedResponseNode& metric) { return !metric.children.empty(); });
  metric_payload.reservePayloads(metric_payload.getNestedPayloads().size() + payloads);
  for (const auto &metric : metrics) {
    if (metric.children.size() > 0) {
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
      logger_->log_debug("Received initiation event from protocol");
      break;
    case state::UpdateState::READ_COMPLETE:
      logger_->log_trace("Received Ack from Server");
      // we have a heartbeat response.
      for (const C2ContentResponse &server_response : resp.getContent()) {
        handle_c2_server_response(server_response);
      }
      break;
    case state::UpdateState::FULLY_APPLIED:
      logger_->log_debug("Received fully applied event from protocol");
      break;
    case state::UpdateState::PARTIALLY_APPLIED:
      logger_->log_debug("Received partially applied event from protocol");
      break;
    case state::UpdateState::NOT_APPLIED:
      logger_->log_debug("Received not applied event from protocol");
      break;
    case state::UpdateState::SET_ERROR:
      logger_->log_debug("Received error event from protocol");
      break;
    case state::UpdateState::READ_ERROR:
      logger_->log_debug("Received error event from protocol");
      break;
    case state::UpdateState::NESTED:  // multiple updates embedded into one

    default:
      logger_->log_debug("Received nested event from protocol");
      break;
  }
}

void C2Agent::handle_c2_server_response(const C2ContentResponse &resp) {
  switch (resp.op) {
    case Operation::CLEAR:
      // we've been told to clear something
      if (resp.name == "connection") {
        for (const auto& connection : resp.operation_arguments) {
          logger_->log_debug("Clearing connection %s", connection.second.to_string());
          update_sink_->clearConnection(connection.second.to_string());
        }
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      } else if (resp.name == "repositories") {
        update_sink_->drainRepositories();
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      } else if (resp.name == "corecomponentstate") {
        // TODO(bakaid): untested
        std::vector<std::shared_ptr<state::StateController>> components = update_sink_->getComponents(resp.name);
        auto state_manager_provider = core::ProcessContext::getStateManagerProvider(logger_, controller_, configuration_);
        if (state_manager_provider != nullptr) {
          for (auto &component : components) {
            logger_->log_debug("Clearing state for component %s", component->getComponentName());
            auto state_manager = state_manager_provider->getCoreComponentStateManager(component->getComponentUUID());
            if (state_manager != nullptr) {
              component->stop();
              state_manager->clear();
              state_manager->persist();
              component->start();
            } else {
              logger_->log_warn("Failed to get StateManager for component %s", component->getComponentUUID().to_string());
            }
          }
        } else {
          logger_->log_error("Failed to get StateManagerProvider");
        }
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      } else {
        logger_->log_debug("Clearing unknown %s", resp.name);
      }

      break;
    case Operation::UPDATE:
      handle_update(resp);
      break;
    case Operation::DESCRIBE:
      handle_describe(resp);
      break;
    case Operation::RESTART: {
      update_sink_->stop();
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
      protocol_.load()->consumePayload(std::move(response));
      restart_agent();
    }
      break;
    case Operation::START:
    case Operation::STOP: {
      if (resp.name == "C2" || resp.name == "c2") {
        raise(SIGTERM);
      }

      std::vector<std::shared_ptr<state::StateController>> components = update_sink_->getComponents(resp.name);
      // stop all referenced components.
      for (auto &component : components) {
        logger_->log_debug("Stopping component %s", component->getComponentName());
        if (resp.op == Operation::STOP)
          component->stop();
        else
          component->start();
      }

      if (resp.ident.length() > 0) {
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      }
    }
      //
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
            [](std::string key) {return key.find("pass") == std::string::npos;});

    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
    C2Payload options(Operation::ACKNOWLEDGE);
    options.setLabel("configuration_options");
    std::string value;
    for (auto key : keys) {
      C2ContentResponse option(Operation::ACKNOWLEDGE);
      option.name = key;
      if (configuration_->get(key, value)) {
        option.operation_arguments[key] = value;
        options.addContent(std::move(option));
      }
    }
    response.addPayload(std::move(options));
    return response;
}

/**
 * Descriptions are special types of requests that require information
 * to be put into the acknowledgement
 */
void C2Agent::handle_describe(const C2ContentResponse &resp) {
  auto reporter = std::dynamic_pointer_cast<state::response::NodeReporter>(update_sink_);
  if (resp.name == "metrics") {
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
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
        serializeMetrics(metrics, metricsNode->getName(), metricsNode->serialize(), metricsNode->isArray());
      }
      response.addPayload(std::move(metrics));
    }
    enqueue_c2_response(std::move(response));
    return;
  } else if (resp.name == "configuration") {
    auto configOptions = prepareConfigurationOptions(resp);
    enqueue_c2_response(std::move(configOptions));
    return;
  } else if (resp.name == "manifest") {
    C2Payload response(prepareConfigurationOptions(resp));
    if (reporter != nullptr) {
      C2Payload agentInfo(Operation::ACKNOWLEDGE, resp.ident, false, true);
      agentInfo.setLabel("agentInfo");

      const auto manifest = reporter->getAgentManifest();
      serializeMetrics(agentInfo, manifest->getName(), manifest->serialize());
      response.addPayload(std::move(agentInfo));
    }
    enqueue_c2_response(std::move(response));
    return;
  } else if (resp.name == "jstack") {
    if (update_sink_->isRunning()) {
      const std::vector<BackTrace> traces = update_sink_->getTraces();
      for (const auto &trace : traces) {
        for (const auto & line : trace.getTraces()) {
          logger_->log_trace("%s -- %s", trace.getName(), line);
        }
      }
      auto keys = configuration_->getConfiguredKeys();
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
      for (const auto &trace : traces) {
        C2Payload options(Operation::ACKNOWLEDGE, resp.ident, false, true);
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
      return;
    }
  } else if (resp.name == "corecomponentstate") {
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
    response.setLabel("corecomponentstate");
    C2Payload states(Operation::ACKNOWLEDGE, resp.ident, false, true);
    states.setLabel("corecomponentstate");
    auto state_manager_provider = core::ProcessContext::getStateManagerProvider(logger_, controller_, configuration_);
    if (state_manager_provider != nullptr) {
      auto core_component_states = state_manager_provider->getAllCoreComponentStates();
      for (const auto& core_component_state : core_component_states) {
        C2Payload state(Operation::ACKNOWLEDGE, resp.ident, false, true);
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
    return;
  }
  C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
  enqueue_c2_response(std::move(response));
}

void C2Agent::handle_update(const C2ContentResponse &resp) {
  // we've been told to update something
  if (resp.name == "configuration") {
    handleConfigurationUpdate(resp);
  } else if (resp.name == "properties") {
    bool update_occurred = false;
    for (auto entry : resp.operation_arguments) {
      if (update_property(entry.first, entry.second.to_string()))
        update_occurred = true;
    }
    if (update_occurred) {
      // enable updates to persist the configuration.
    }
  } else if (resp.name == "c2") {
    // prior configuration options were already in place. thus
    // we clear the map so that we don't go through replacing
    // unnecessary objects.
    running_c2_configuration->clear();

    for (auto entry : resp.operation_arguments) {
      bool can_update = true;
      if (nullptr != update_service_) {
        can_update = update_service_->canUpdate(entry.first);
      }
      if (can_update)
        running_c2_configuration->set(entry.first, entry.second.to_string());
    }

    if (resp.operation_arguments.size() > 0)
      configure(running_c2_configuration);
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, false, true);
    enqueue_c2_response(std::move(response));
  } else if (resp.name == "agent") {
    // we are upgrading the agent. therefore we must be given a location
    auto location = resp.operation_arguments.find("location");
    auto isPartialStr = resp.operation_arguments.find("partial");

    bool partial_update = false;
    if (isPartialStr != std::end(resp.operation_arguments)) {
      partial_update = utils::StringUtils::equalsIgnoreCase(isPartialStr->second.to_string(), "true");
    }
    if (location != resp.operation_arguments.end()) {
      logger_->log_trace("Update agent with location %s", location->second.to_string());
      // we will not have a raw payload
      C2Payload payload(Operation::TRANSFER, false, true);

      C2Payload &&response = protocol_.load()->consumePayload(location->second.to_string(), payload, RECEIVE, false);

      auto raw_data = response.getRawData();

      std::string file_path = std::string(raw_data.data(), raw_data.size());

      logger_->log_trace("Update requested with file %s", file_path);

      // acknowledge the transfer. For a transfer, the response identifier should be the checksum of the
      // file transferred.
      C2Payload transfer_response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, response.getIdentifier(), false, true);

      protocol_.load()->consumePayload(std::move(transfer_response));

      if (allow_updates_) {
        logger_->log_trace("Update allowed from file %s", file_path);
        if (partial_update && !bin_location_.empty()) {
          utils::file::DiffUtils::apply_binary_diff(bin_location_.c_str(), file_path.c_str(), update_location_.c_str());
        } else {
          utils::file::FileUtils::copy_file(file_path, update_location_);
        }
        // remove the downloaded file.
        logger_->log_trace("removing file %s", file_path);
        std::remove(file_path.c_str());
        update_agent();
      } else {
        logger_->log_trace("Update disallowed from file %s", file_path);
      }

    } else {
      logger_->log_trace("No location present");
    }
  } else {
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::NOT_APPLIED, resp.ident, false, true);
    enqueue_c2_response(std::move(response));
  }
}

/**
 * Updates a property
 */
bool C2Agent::update_property(const std::string &property_name, const std::string &property_value, bool persist) {
  if (update_service_->canUpdate(property_name)) {
    configuration_->set(property_name, property_value);
    if (persist) {
      configuration_->persistProperties();
      return true;
    }
  }
  return false;
}

void C2Agent::restart_agent() {
  std::string cwd = utils::Environment::getCurrentWorkingDirectory();
  if (cwd.empty()) {
    logger_->log_error("Could not restart the agent because the working directory could not be determined");
    return;
  }

  std::string command = cwd + "/bin/minifi.sh restart";
  system(command.c_str());
}

void C2Agent::update_agent() {
  if (!system(update_command_.c_str())) {
    logger_->log_warn("May not have command processor");
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

    try {
      performHeartBeat();
    }
    catch (const std::exception &e) {
      logger_->log_error("Exception occurred while performing heartbeat. error: %s", e.what());
    }
    catch (...) {
      logger_->log_error("Unknonwn exception occurred while performing heartbeat.");
    }
  }

  checkTriggers();

  return utils::TaskRescheduleInfo::RetryIn(std::chrono::milliseconds(heart_beat_period_));
}

utils::TaskRescheduleInfo C2Agent::consume() {
  const auto consume_success = responses.consume([this] (C2Payload&& payload) {
    extractPayload(std::move(payload));
  });
  if (!consume_success) {
    extractPayload(C2Payload{ Operation::HEARTBEAT });
  }
  return utils::TaskRescheduleInfo::RetryIn(std::chrono::milliseconds(C2RESPONSE_POLL_MS));
}

utils::optional<std::string> C2Agent::fetchFlow(const std::string& uri) const {
  if (!utils::StringUtils::startsWith(uri, "http") || protocol_.load() == nullptr) {
    // try to open the file
    utils::optional<std::string> content = filesystem_->read(uri);
    if (content) {
      return content;
    }
  }
  // couldn't open as file and we have no protocol to request the file from
  if (protocol_.load() == nullptr) {
    return {};
  }

  std::string resolved_url = uri;
  if (!utils::StringUtils::startsWith(uri, "http")) {
    std::stringstream adjusted_url;
    std::string base;
    if (configuration_->get(minifi::Configure::nifi_c2_flow_base_url, base)) {
      adjusted_url << base;
      if (!utils::StringUtils::endsWith(base, "/")) {
        adjusted_url << "/";
      }
      adjusted_url << uri;
      resolved_url = adjusted_url.str();
    } else if (configuration_->get("c2.rest.url", base)) {
      std::string host, protocol;
      int port = -1;
      utils::parse_url(&base, &host, &port, &protocol);
      adjusted_url << protocol << host;
      if (port > 0) {
        adjusted_url << ":" << port;
      }
      adjusted_url << "/c2/api/" << uri;
      resolved_url = adjusted_url.str();
    }
  }

  C2Payload payload(Operation::TRANSFER, false, true);
  C2Payload &&response = protocol_.load()->consumePayload(resolved_url, payload, RECEIVE, false);

  auto raw_data = response.getRawData();
  return std::string(raw_data.data(), raw_data.size());
}

bool C2Agent::handleConfigurationUpdate(const C2ContentResponse &resp) {
  auto url = resp.operation_arguments.find("location");

  std::string file_uri;
  std::string configuration_str;

  if (url != resp.operation_arguments.end()) {
    file_uri = url->second.to_string();
    utils::optional<std::string> optional_configuration_str = fetchFlow(file_uri);
    if (!optional_configuration_str) {
      logger_->log_debug("Couldn't load new flow configuration from: \"%s\"", file_uri);
      C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, false, true);
      response.setRawData("Error while applying flow. Couldn't load flow configuration.");
      enqueue_c2_response(std::move(response));
      return false;
    }
    configuration_str = optional_configuration_str.value();
  } else {
    logger_->log_debug("Did not have location within %s", resp.ident);
    auto update_text = resp.operation_arguments.find("configuration_data");
    if (update_text == resp.operation_arguments.end()) {
      logger_->log_debug("Neither the config file location nor the data is provided");
      C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, false, true);
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
    logger_->log_debug("Flow configuration update failed with error code %" PRIi16, err);
    C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, false, true);
    response.setRawData("Error while applying flow. Likely missing processors");
    enqueue_c2_response(std::move(response));
    return false;
  }

  C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, false, true);
  enqueue_c2_response(std::move(response));

  if (should_persist) {
    // update the flow id
    configuration_->persistProperties();
  }

  return true;
}

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
