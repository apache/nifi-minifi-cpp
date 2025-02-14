/**
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
#include "RootProcessGroupWrapper.h"

namespace org::apache::nifi::minifi {

void RootProcessGroupWrapper::updatePropertyValue(const std::string& processor_name, const std::string& property_name, const std::string& property_value) {
  if (root_ != nullptr) {
    root_->updatePropertyValue(processor_name, property_name, property_value);
  }
}

std::string RootProcessGroupWrapper::getName() const {
  if (root_ != nullptr) {
    return root_->getName();
  }
  return "";
}

utils::Identifier RootProcessGroupWrapper::getComponentUUID() const {
  if (!root_) {
    return {};
  }
  return root_->getUUID();
}

std::string RootProcessGroupWrapper::getVersion() const {
  if (root_ != nullptr) {
    return std::to_string(root_->getVersion());
  }
  return "0";
}

void RootProcessGroupWrapper::setNewRoot(std::unique_ptr<core::ProcessGroup> new_root) {
  if (!new_root) {
    logger_->log_error("New flow to be set was empty!");
    return;
  }

  if (metrics_publisher_store_) { metrics_publisher_store_->clearMetricNodes(); }
  backup_root_ = std::move(root_);
  root_ = std::move(new_root);
  processor_to_controller_.clear();
  if (metrics_publisher_store_) { metrics_publisher_store_->loadMetricNodes(root_.get()); }
}

void RootProcessGroupWrapper::restoreBackup() {
  if (metrics_publisher_store_) { metrics_publisher_store_->clearMetricNodes(); }
  root_ = std::move(backup_root_);
  processor_to_controller_.clear();
  if (metrics_publisher_store_) { metrics_publisher_store_->loadMetricNodes(root_.get()); }
}

void RootProcessGroupWrapper::clearBackup() {
  backup_root_ = nullptr;
}

void RootProcessGroupWrapper::stopProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                          EventDrivenSchedulingAgent& event_scheduler,
                          CronDrivenSchedulingAgent& cron_scheduler) {
  if (!root_) {
    return;
  }
  // stop source processors first
  root_->stopProcessing(timer_scheduler, event_scheduler, cron_scheduler, [] (const core::Processor* proc) -> bool {
    return !proc->hasIncomingConnections();
  });
  // we enable C2 to progressively increase the timeout
  // in case it sees that waiting for a little longer could
  // allow the FlowFiles to be processed
  auto shutdown_start = std::chrono::steady_clock::now();
  while ((std::chrono::steady_clock::now() - shutdown_start) < loadShutdownTimeoutFromConfiguration().value_or(std::chrono::milliseconds{0}) &&
      root_->getTotalFlowFileCount() != 0) {
    std::this_thread::sleep_for(shutdown_check_interval_);
  }
  // shutdown all other processors as well
  root_->stopProcessing(timer_scheduler, event_scheduler, cron_scheduler);
}

void RootProcessGroupWrapper::drainConnections() {
  if (root_) {
    root_->drainConnections();
  }
}

void RootProcessGroupWrapper::getConnections(std::map<std::string, core::Connectable*>& connectionMap) {
  if (root_) {
    root_->getConnections(connectionMap);
  }
}

void RootProcessGroupWrapper::getFlowFileContainers(std::map<std::string, core::Connectable*>& containers) const {
  if (root_) {
    root_->getFlowFileContainers(containers);
  }
}

bool RootProcessGroupWrapper::startProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                           EventDrivenSchedulingAgent& event_scheduler,
                           CronDrivenSchedulingAgent& cron_scheduler) {
  if (!root_) {
    return false;
  }

  root_->startProcessing(timer_scheduler, event_scheduler, cron_scheduler);
  return true;
}

void RootProcessGroupWrapper::clearConnection(const std::string &connection) {
  if (root_ == nullptr) {
    return;
  }
  logger_->log_info("Attempting to clear connection {}", connection);
  std::map<std::string, Connection*> connections;
  root_->getConnections(connections);
  auto conn = connections.find(connection);
  if (conn != connections.end()) {
    logger_->log_info("Clearing connection {}", connection);
    conn->second->drain(true);
  }
}

std::optional<std::vector<state::StateController*>> RootProcessGroupWrapper::getAllProcessorControllers(
    const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory) {
  if (!root_) {
    return std::nullopt;
  }
  std::vector<state::StateController*> controllerVec;
  std::vector<core::Processor*> processorVec;
  root_->getAllProcessors(processorVec);

  for (const auto& processor : processorVec) {
    // reference to the existing or newly created controller
    auto& controller = processor_to_controller_[processor->getUUID()];
    if (!controller) {
      controller = controllerFactory(*processor);
    }
    controllerVec.push_back(controller.get());
  }

  return controllerVec;
}

state::StateController* RootProcessGroupWrapper::getProcessorController(const std::string& id_or_name,
    const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory) {
  if (!root_) {
    return nullptr;
  }

  return utils::Identifier::parse(id_or_name)
    | utils::andThen([this](utils::Identifier id) { return utils::optional_from_ptr(root_->findProcessorById(id)); })
    | utils::orElse([this, &id_or_name] { return utils::optional_from_ptr(root_->findProcessorByName(id_or_name)); })
    | utils::transform([this, &controllerFactory](gsl::not_null<core::Processor*> proc) -> gsl::not_null<state::ProcessorController*> {
      return utils::optional_from_ptr(processor_to_controller_[proc->getUUID()].get())
          | utils::valueOrElse([this, proc, &controllerFactory] {
            return gsl::make_not_null((processor_to_controller_[proc->getUUID()] = controllerFactory(*proc)).get());
          });
    })
    | utils::valueOrElse([this, &id_or_name]() -> state::ProcessorController* {
      logger_->log_error("Could not get processor controller for requested id/name \"{}\", because the processor was not found", id_or_name);
      return nullptr;
    });
}

std::optional<std::chrono::milliseconds> RootProcessGroupWrapper::loadShutdownTimeoutFromConfiguration() {
  std::string shutdown_timeout_str;
  if (configuration_->get(minifi::Configure::nifi_flowcontroller_drain_timeout, shutdown_timeout_str)) {
    if (const auto shutdown_timeout = parsing::parseDuration<std::chrono::milliseconds>(shutdown_timeout_str)) {
      return *shutdown_timeout;
    }
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi
