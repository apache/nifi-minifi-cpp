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
#include <unistd.h>
#include <csignal>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include "c2/ControllerSocketProtocol.h"
#include "core/state/UpdateController.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileManager.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2Agent::C2Agent(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                 const std::shared_ptr<Configure> &configuration)
    : controller_(controller),
      update_sink_(updateSink),
      configuration_(configuration),
      heart_beat_period_(3000),
      max_c2_responses(5),
      logger_(logging::LoggerFactory<C2Agent>::getLogger()) {
  allow_updates_ = true;

  running_configuration = std::make_shared<Configure>();

  last_run_ = std::chrono::steady_clock::now();

  configure(configuration, false);

  c2_producer_ = [&]() {
    auto now = std::chrono::steady_clock::now();
    auto time_since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_run_).count();

    // place priority on messages to send to the c2 server
      if ( request_mutex.try_lock_until(now + std::chrono::seconds(1)) ) {
        if (requests.size() > 0) {
          int count = 0;
          do {
            const C2Payload payload(std::move(requests.back()));
            requests.pop_back();
            C2Payload && response = protocol_.load()->consumePayload(payload);
            enqueue_c2_server_response(std::move(response));
          }while(requests.size() > 0 && ++count < max_c2_responses);
        }
        request_mutex.unlock();
      }

      if ( time_since > heart_beat_period_ ) {
        last_run_ = now;
        performHeartBeat();
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

void C2Agent::configure(const std::shared_ptr<Configure> &configure, bool reconfigure) {
  std::string clazz, heartbeat_period, device;

  if (!reconfigure) {
    if (!configure->get("c2.agent.protocol.class", clazz)) {
      clazz = "RESTSender";
    }
    logger_->log_info("Class is %s", clazz);
    auto protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw(clazz, clazz);

    if (protocol == nullptr) {
      logger_->log_info("Class %s not found", clazz);
      protocol = core::ClassLoader::getDefaultClassLoader().instantiateRaw("RESTSender", "RESTSender");

      if (!protocol) {
        return;
      }
      logger_->log_info("Class is RESTSender");
    }
    C2Protocol *old_protocol = protocol_.exchange(dynamic_cast<C2Protocol*>(protocol));

    protocol_.load()->initialize(controller_, configuration_);

    if (reconfigure && old_protocol != nullptr) {
      delete old_protocol;
    }
  } else {
    protocol_.load()->update(configure);
  }

  if (configure->get("c2.agent.heartbeat.period", heartbeat_period)) {
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
  if (configure->get("c2.agent.update.allow", update_settings) && utils::StringUtils::StringToBool(update_settings, allow_updates_)) {
    // allow the agent to be updated. we then need to get an update command to execute after
  }

  if (allow_updates_) {
    if (!configure->get("c2.agent.update.command", update_command_)) {
      char cwd[1024];
      getcwd(cwd, sizeof(cwd));

      std::stringstream command;
      command << cwd << "/minifi.sh update";
      update_command_ = command.str();
    }

    if (!configure->get("c2.agent.update.temp.location", update_location_)) {
      char cwd[1024];
      getcwd(cwd, sizeof(cwd));

      std::stringstream copy_path;
      std::stringstream command;

      copy_path << cwd << "/minifi.update";
    }
  }
  std::string heartbeat_reporters;
  if (configure->get("c2.agent.heartbeat.reporter.classes", heartbeat_reporters)) {
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

void C2Agent::serializeMetrics(C2Payload &metric_payload, const std::string &name, const std::vector<state::response::SerializedResponseNode> &metrics, bool is_container) {
  for (auto metric : metrics) {
    if (metric.children.size() > 0) {
      C2Payload child_metric_payload(metric_payload.getOperation());
      child_metric_payload.setLabel(metric.name);
      serializeMetrics(child_metric_payload, metric.name, metric.children, is_container);
      metric_payload.addPayload(std::move(child_metric_payload));
    } else {
      C2ContentResponse response(metric_payload.getOperation());
      response.name = name;
      response.operation_arguments[metric.name] = metric.value;
      metric_payload.addContent(std::move(response));
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
      C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
      enqueue_c2_response(std::move(response));
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

      C2Payload &&response = protocol_.load()->consumePayload(url->second.to_string(), payload, RECEIVE, false);

      auto raw_data = response.getRawData();
      std::string file_path = std::string(raw_data.data(), raw_data.size());

      std::ifstream new_conf(file_path);
      std::string raw_data_str((std::istreambuf_iterator<char>(new_conf)), std::istreambuf_iterator<char>());
      unlink(file_path.c_str());
      // if we can apply the update, we will acknowledge it and then backup the configuration file.
      if (update_sink_->applyUpdate(url->second.to_string(), raw_data_str)) {
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));

        if (persist != resp.operation_arguments.end() && utils::StringUtils::equalsIgnoreCase(persist->second.to_string(), "true")) {
          // update nifi.flow.configuration.file=./conf/config.yml
          std::string config_file;
          configuration_->get(minifi::Configure::nifi_flow_configuration_file, config_file);
          std::stringstream config_file_backup;
          config_file_backup << config_file << ".bak";
          // we must be able to successfuly copy the file.
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
              auto output = response.getRawData();
              writer.write(output.data(), output.size());
            }
            writer.close();
          }
        }
      } else {
        logger_->log_debug("update failed.");
        C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
        enqueue_c2_response(std::move(response));
      }
      // send
    } else {
      auto update_text = resp.operation_arguments.find("configuration_data");
      if (update_text != resp.operation_arguments.end()) {
        if (update_sink_->applyUpdate(url->second.to_string(), update_text->second.to_string()) != 0 && persist != resp.operation_arguments.end()
            && utils::StringUtils::equalsIgnoreCase(persist->second.to_string(), "true")) {
          C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
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
          C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
          enqueue_c2_response(std::move(response));
        }
      }
    }
  } else if (resp.name == "c2") {
    // prior configuration options were already in place. thus
    // we clear the map so that we don't go through replacing
    // unnecessary objects.
    running_configuration->clear();

    for (auto entry : resp.operation_arguments) {
      running_configuration->set(entry.first, entry.second.to_string());
    }

    if (resp.operation_arguments.size() > 0)
      configure(running_configuration);
    C2Payload response(Operation::ACKNOWLEDGE, resp.ident, false, true);
    enqueue_c2_response(std::move(response));
  } else if (resp.name == "agent") {
    // we are upgrading the agent. therefore we must be given a location
    auto location = resp.operation_arguments.find("location");
    if (location != resp.operation_arguments.end()) {
      // we will not have a raw payload
      C2Payload payload(Operation::TRANSFER, false, true);

      C2Payload &&response = protocol_.load()->consumePayload(location->second.to_string(), payload, RECEIVE, false);

      auto raw_data = response.getRawData();

      std::string file_path = std::string(raw_data.data(), raw_data.size());

      // acknowledge the transfer. For a transfer, the response identifier should be the checksum of the
      // file transferred.
      C2Payload transfer_response(Operation::ACKNOWLEDGE, response.getIdentifier(), false, true);

      protocol_.load()->consumePayload(std::move(transfer_response));

      if (allow_updates_) {
        utils::file::FileUtils::copy_file(file_path, update_location_);
        // remove the downloaded file.
        logger_->log_trace("removing command %s", file_path);
        unlink(file_path.c_str());
        update_agent();
      }
    }
  }
}

void C2Agent::restart_agent() {
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));

  std::stringstream command;
  command << cwd << "/minifi.sh restart";
}

void C2Agent::update_agent() {
  system(update_command_.c_str());
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
