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
#include <csignal>
#include <utility>
#include <limits>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include "c2/ControllerSocketProtocol.h"
#include "core/state/UpdateController.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/DiffUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileManager.h"
#include "utils/HTTPClient.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2Agent::C2Agent(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                 const std::shared_ptr<Configure> &configuration)
    : controller_(controller),
      update_sink_(updateSink),
      update_service_(nullptr),
      configuration_(configuration),
      heart_beat_period_(3000),
      max_c2_responses(5),
      logger_(logging::LoggerFactory<C2Agent>::getLogger()) {
  allow_updates_ = true;

  running_c2_configuration = std::make_shared<Configure>();

  last_run_ = std::chrono::steady_clock::now();

  if (nullptr != controller_) {
    update_service_ = std::static_pointer_cast<controllers::UpdatePolicyControllerService>(controller_->getControllerService(C2_AGENT_UPDATE_NAME));
  }

  if (update_service_ == nullptr) {
    // create a stubbed service for updating the flow identifier
  }

  configure(configuration, false);

  c2_producer_ = [&]() {
    auto now = std::chrono::steady_clock::now();
    auto time_since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_run_).count();

    // place priority on messages to send to the c2 server
      if ( protocol_.load() != nullptr && request_mutex.try_lock_until(now + std::chrono::seconds(1)) ) {
        if (requests.size() > 0) {
          int count = 0;
          do {
            const C2Payload payload(std::move(requests.back()));
            requests.pop_back();
            try {
              C2Payload && response = protocol_.load()->consumePayload(payload);
              enqueue_c2_server_response(std::move(response));
            }
            catch(const std::exception &e) {
              logger_->log_error("Exception occurred while consuming payload. error: %s", e.what());
            }
            catch(...) {
              logger_->log_error("Unknonwn exception occurred while consuming payload.");
            }
          }while(requests.size() > 0 && ++count < max_c2_responses);
        }
        request_mutex.unlock();
      }

      if ( time_since > heart_beat_period_ ) {
        last_run_ = now;
        try {
          performHeartBeat();
        }
        catch(const std::exception &e) {
          logger_->log_error("Exception occurred while performing heartbeat. error: %s", e.what());
        }
        catch(...) {
          logger_->log_error("Unknonwn exception occurred while performing heartbeat.");
        }
      }

      checkTriggers();

      std::this_thread::sleep_for(std::chrono::milliseconds(heart_beat_period_ > 500 ? 500 : heart_beat_period_));
      return state::Update(state::UpdateStatus(state::UpdateState::READ_COMPLETE, false));
    };

  functions_.push_back(c2_producer_);

  c2_consumer_ = [&]() {
    auto now = std::chrono::steady_clock::now();
    if ( queue_mutex.try_lock_until(now + std::chrono::seconds(1)) ) {
      if (responses.size() > 0) {
        const C2Payload payload(std::move(responses.back()));
        responses.pop_back();
        extractPayload(std::move(payload));
      }
      queue_mutex.unlock();
    }
    return state::Update(state::UpdateStatus(state::UpdateState::READ_COMPLETE, false));
  };

  functions_.push_back(c2_consumer_);
}

void C2Agent::checkTriggers() {
  logger_->log_info("Checking %d triggers", triggers_.size());
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
      logger_->log_info("Class %s not found", clazz);
      protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw("CoapProtocol", "CoapProtocol");
      if (!protocol) {
        logger_->log_info("Attempted to load CoapProtocol. To enable C2, please specify an active protocol for this agent.");
        return;
      } else {
        logger_->log_info("Class is CoapProtocol");
      }
    }
    C2Protocol *old_protocol = protocol_.exchange(dynamic_cast<C2Protocol*>(protocol));

    protocol_.load()->initialize(controller_, configuration_);

    if (reconfigure && old_protocol != nullptr) {
      delete old_protocol;
    }
  } else {
    protocol_.load()->update(configure);
  }

  if (configure->get("nifi.c2.agent.heartbeat.period", "c2.agent.heartbeat.period", heartbeat_period)) {
    try {
      heart_beat_period_ = std::stoi(heartbeat_period);
    } catch (const std::invalid_argument &ie) {
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
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        logger_->log_error("Could not set update command, reason %s", std::strerror(errno));

      } else {
        std::stringstream command;
        command << cwd << "/minifi.sh update";
        update_command_ = command.str();
      }
    }

    if (!configure->get("nifi.c2.agent.update.temp.location", "c2.agent.update.temp.location", update_location_)) {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        logger_->log_error("Could not set copy path, reason %s", std::strerror(errno));
      } else {
        std::stringstream copy_path;
        std::stringstream command;
        copy_path << cwd << "/minifi.update";
      }
    }

    // if not defined we won't beable to update
    configure->get("nifi.c2.agent.bin.location", "c2.agent.bin.location", bin_location_);
  }
  std::string heartbeat_reporters;
  if (configure->get("nifi.c2.agent.heartbeat.reporter.classes", "c2.agent.heartbeat.reporter.classes", heartbeat_reporters)) {
    std::vector<std::string> reporters = utils::StringUtils::split(heartbeat_reporters, ",");
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    for (auto reporter : reporters) {
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
    for (auto trigger : triggers) {
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

  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> metrics_copy;
  {
    std::lock_guard<std::timed_mutex> lock(metrics_mutex_);
    if (metrics_map_.size() > 0) {
      metrics_copy = std::move(metrics_map_);
    }
  }

  if (metrics_copy.size() > 0) {
    C2Payload metrics(Operation::HEARTBEAT);
    metrics.setLabel("metrics");

    for (auto metric : metrics_copy) {
      if (metric.second->serialize().size() == 0)
        continue;
      C2Payload child_metric_payload(Operation::HEARTBEAT);
      child_metric_payload.setLabel(metric.first);
      serializeMetrics(child_metric_payload, metric.first, metric.second->serialize(), metric.second->isArray());
      metrics.addPayload(std::move(child_metric_payload));
    }
    payload.addPayload(std::move(metrics));
  }

  if (device_information_.size() > 0) {
    C2Payload deviceInfo(Operation::HEARTBEAT);
    deviceInfo.setLabel("AgentInformation");

    for (auto metric : device_information_) {
      C2Payload child_metric_payload(Operation::HEARTBEAT);
      child_metric_payload.setLabel(metric.first);
      if (metric.second->isArray()) {
        child_metric_payload.setContainer(true);
      }
      serializeMetrics(child_metric_payload, metric.first, metric.second->serialize(), metric.second->isArray());
      deviceInfo.addPayload(std::move(child_metric_payload));
    }
    payload.addPayload(std::move(deviceInfo));
  }

  if (!root_response_nodes_.empty()) {
    for (auto metric : root_response_nodes_) {
      C2Payload child_metric_payload(Operation::HEARTBEAT);
      child_metric_payload.setLabel(metric.first);
      if (metric.second->isArray()) {
        child_metric_payload.setContainer(true);
      }
      serializeMetrics(child_metric_payload, metric.first, metric.second->serialize(), metric.second->isArray());
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

void C2Agent::extractPayload(const C2Payload &&resp) {
  if (resp.getStatus().getState() == state::UpdateState::NESTED) {
    const std::vector<C2Payload> &payloads = resp.getNestedPayloads();

    for (const auto &payload : payloads) {
      extractPayload(std::move(payload));
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
      for (const auto &server_response : resp.getContent()) {
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

void C2Agent::extractPayload(const C2Payload &resp) {
  if (resp.getStatus().getState() == state::UpdateState::NESTED) {
    const std::vector<C2Payload> &payloads = resp.getNestedPayloads();
    for (const auto &payload : payloads) {
      extractPayload(payload);
    }
  }
  switch (resp.getStatus().getState()) {
    case state::UpdateState::READ_COMPLETE:
      // we have a heartbeat response.
      for (const auto &server_response : resp.getContent()) {
        handle_c2_server_response(server_response);
      }
      break;
    default:
      break;
  }
}

void C2Agent::handle_c2_server_response(const C2ContentResponse &resp) {
  switch (resp.op) {
    case Operation::CLEAR:
      // we've been told to clear something
      if (resp.name == "connection") {
        for (auto connection : resp.operation_arguments) {
          logger_->log_debug("Clearing connection %s", connection.second.to_string());
          update_sink_->clearConnection(connection.second.to_string());
        }
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      } else if (resp.name == "repositories") {
        update_sink_->drainRepositories();
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      } else {
        logger_->log_debug("Clearing unknown %s", resp.name);
      }

      break;
    case Operation::UPDATE: {
      handle_update(resp);
    }
      break;

    case Operation::DESCRIBE:
      handle_describe(resp);
      break;
    case Operation::RESTART: {
      update_sink_->stop(true);
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
      protocol_.load()->consumePayload(std::move(response));
      exit(1);
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
          component->stop(true);
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

/**
 * Descriptions are special types of requests that require information
 * to be put into the acknowledgement
 */
void C2Agent::handle_describe(const C2ContentResponse &resp) {
  if (resp.name == "metrics") {
    auto reporter = std::dynamic_pointer_cast<state::response::NodeReporter>(update_sink_);

    if (reporter != nullptr) {
      auto metricsClass = resp.operation_arguments.find("metricsClass");
      uint8_t metric_class_id = 0;
      if (metricsClass != resp.operation_arguments.end()) {
        // we have a class
        try {
          metric_class_id = std::stoi(metricsClass->second.to_string());
        } catch (...) {
          logger_->log_error("Could not convert %s into an integer", metricsClass->second.to_string());
        }
      }

      std::vector<std::shared_ptr<state::response::ResponseNode>> metrics_vec;

      reporter->getResponseNodes(metrics_vec, metric_class_id);
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
      response.setLabel("metrics");
      for (auto metric : metrics_vec) {
        serializeMetrics(response, metric->getName(), metric->serialize());
      }
      enqueue_c2_response(std::move(response));
    }

  } else if (resp.name == "configuration") {
    auto unsanitized_keys = configuration_->getConfiguredKeys();
    std::vector<std::string> keys;
    std::copy_if(unsanitized_keys.begin(), unsanitized_keys.end(), std::back_inserter(keys), [](std::string key) {return key.find("pass") == std::string::npos;});
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
    response.setLabel("configuration_options");
    C2Payload options(Operation::ACKNOWLEDGE, resp.ident, false, true);
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
    enqueue_c2_response(std::move(response));
    return;
  } else if (resp.name == "manifest") {
    auto keys = configuration_->getConfiguredKeys();
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
    response.setLabel("configuration_options");
    C2Payload options(Operation::ACKNOWLEDGE, resp.ident, false, true);
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

    if (device_information_.size() > 0) {
      C2Payload deviceInfo(Operation::HEARTBEAT);
      deviceInfo.setLabel("AgentInformation");

      for (auto metric : device_information_) {
        C2Payload child_metric_payload(Operation::HEARTBEAT);
        child_metric_payload.setLabel(metric.first);
        if (metric.second->isArray()) {
          child_metric_payload.setContainer(true);
        }
        serializeMetrics(child_metric_payload, metric.first, metric.second->serialize(), metric.second->isArray());
        deviceInfo.addPayload(std::move(child_metric_payload));
      }
      response.addPayload(std::move(deviceInfo));
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
      response.setLabel("configuration_options");
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
    }
  }
  C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
  enqueue_c2_response(std::move(response));
}

void C2Agent::handle_update(const C2ContentResponse &resp) {
  // we've been told to update something
  if (resp.name == "configuration") {
    auto url = resp.operation_arguments.find("location");

    auto persist = resp.operation_arguments.find("persist");

    if (url != resp.operation_arguments.end()) {
      // just get the raw data.
      C2Payload payload(Operation::TRANSFER, false, true);

      auto urlStr = url->second.to_string();

      std::string file_path = urlStr;
      bool containsHttp = file_path.find("http") != std::string::npos;
      if (!containsHttp) {
        std::ifstream new_conf(file_path);
        if (!new_conf.good()) {
          containsHttp = true;
        }
      }
      if (nullptr != protocol_.load() && containsHttp) {
        std::stringstream newUrl;
        if (urlStr.find("http") == std::string::npos) {
          std::string base;
          if (configuration_->get(minifi::Configure::nifi_c2_flow_base_url, base)) {
            newUrl << base;
            if (!utils::StringUtils::endsWith(base, "/")) {
              newUrl << "/";
            }
            newUrl << urlStr;
            urlStr = newUrl.str();
          } else if (configuration_->get("c2.rest.url", base)) {
            std::string host, protocol;
            int port = -1;
            utils::parse_url(&base, &host, &port, &protocol);
            newUrl << protocol << host;
            if (port > 0) {
              newUrl << ":" << port;
            }
            newUrl << "/c2/api/" << urlStr;
            urlStr = newUrl.str();
          }
        }

        C2Payload &&response = protocol_.load()->consumePayload(urlStr, payload, RECEIVE, false);

        auto raw_data = response.getRawData();
        file_path = std::string(raw_data.data(), raw_data.size());
      }

      std::ifstream new_conf(file_path);
      std::string raw_data_str((std::istreambuf_iterator<char>(new_conf)), std::istreambuf_iterator<char>());
      unlink(file_path.c_str());
      // if we can apply the update, we will acknowledge it and then backup the configuration file.
      if (update_sink_->applyUpdate(urlStr, raw_data_str)) {
        C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, false, true);
        enqueue_c2_response(std::move(response));

        if (persist != resp.operation_arguments.end() && utils::StringUtils::equalsIgnoreCase(persist->second.to_string(), "true")) {
          // update nifi.flow.configuration.file=./conf/config.yml
          std::string config_file;

          configuration_->get(minifi::Configure::nifi_flow_configuration_file, config_file);
          std::string adjustedFilename;
          if (config_file[0] != '/') {
            adjustedFilename = adjustedFilename + configuration_->getHome() + "/" + config_file;
          } else {
            adjustedFilename += config_file;
          }

          config_file = adjustedFilename;

          std::stringstream config_file_backup;
          config_file_backup << config_file << ".bak";
          // we must be able to successfully copy the file.
          bool persist_config = true;
          bool backup_file = false;
          std::string backup_config;

          if (configuration_->get(minifi::Configure::nifi_flow_configuration_file_backup_update, backup_config) && utils::StringUtils::StringToBool(backup_config, backup_file)) {
            if (utils::file::FileUtils::copy_file(config_file, config_file_backup.str()) != 0) {
              logger_->log_debug("Cannot copy %s to %s", config_file, config_file_backup.str());
              persist_config = false;
            }
          }
          logger_->log_debug("Copy %s to %s %d", config_file, config_file_backup.str(), persist_config);
          if (persist_config) {
            std::ofstream writer(config_file);
            if (writer.is_open()) {
              writer.write(raw_data_str.data(), raw_data_str.size());
            }
            writer.close();

            // update the flow id
            configuration_->persistProperties();
          }
        }
      } else {
        logger_->log_debug("update failed.");
        C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      }
      // send
    } else {
      logger_->log_debug("Did not have location within %s", resp.ident);
      auto update_text = resp.operation_arguments.find("configuration_data");
      if (update_text != resp.operation_arguments.end()) {
        if (update_sink_->applyUpdate(url->second.to_string(), update_text->second.to_string()) != 0 && persist != resp.operation_arguments.end()
            && utils::StringUtils::equalsIgnoreCase(persist->second.to_string(), "true")) {
          C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::FULLY_APPLIED, resp.ident, false, true);
          enqueue_c2_response(std::move(response));
          // update nifi.flow.configuration.file=./conf/config.yml
          std::string config_file;
          std::stringstream config_file_backup;
          config_file_backup << config_file << ".bak";

          bool persist_config = true;
          bool backup_file = false;
          std::string backup_config;

          if (configuration_->get(minifi::Configure::nifi_flow_configuration_file_backup_update, backup_config) && utils::StringUtils::StringToBool(backup_config, backup_file)) {
            if (utils::file::FileUtils::copy_file(config_file, config_file_backup.str()) != 0) {
              persist_config = false;
            }
          }
          if (persist_config) {
            std::ofstream writer(config_file);
            if (writer.is_open()) {
              auto output = update_text->second.to_string();
              writer.write(output.c_str(), output.size());
            }
            writer.close();
          }
        } else {
          C2Payload response(Operation::ACKNOWLEDGE, state::UpdateState::SET_ERROR, resp.ident, false, true);
          enqueue_c2_response(std::move(response));
        }
      }
    }
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
        unlink(file_path.c_str());
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
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == nullptr) {
    logger_->log_error("Could not restart agent, reason %s", std::strerror(errno));
    return;
  }

  std::stringstream command;
  command << cwd << "/minifi.sh restart";
}

void C2Agent::update_agent() {
  if (!system(update_command_.c_str())) {
    logger_->log_warn("May not have command processor");
  }
}

int16_t C2Agent::setResponseNodes(const std::shared_ptr<state::response::ResponseNode> &metric) {
  auto now = std::chrono::steady_clock::now();
  if (metrics_mutex_.try_lock_until(now + std::chrono::seconds(1))) {
    root_response_nodes_[metric->getName()] = metric;
    metrics_mutex_.unlock();
    return 0;
  }
  return -1;
}

int16_t C2Agent::setMetricsNodes(const std::shared_ptr<state::response::ResponseNode> &metric) {
  auto now = std::chrono::steady_clock::now();
  if (metrics_mutex_.try_lock_until(now + std::chrono::seconds(1))) {
    metrics_map_[metric->getName()] = metric;
    metrics_mutex_.unlock();
    return 0;
  }
  return -1;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
