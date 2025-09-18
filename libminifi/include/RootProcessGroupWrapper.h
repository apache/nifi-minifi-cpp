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
#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "core/ProcessGroup.h"
#include "utils/Id.h"
#include "core/state/ProcessorController.h"
#include "core/state/MetricsPublisherStore.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "c2/ControllerSocketProtocol.h"

namespace org::apache::nifi::minifi {

class RootProcessGroupWrapper {
 public:
  explicit RootProcessGroupWrapper(std::shared_ptr<Configure> configuration, state::MetricsPublisherStore* metrics_publisher_store = nullptr)
    : configuration_(std::move(configuration)),
      metrics_publisher_store_(metrics_publisher_store) {
  }

  ~RootProcessGroupWrapper() {
    if (metrics_publisher_store_) {
      metrics_publisher_store_->clearMetricNodes();
    }
  }

  void updatePropertyValue(const std::string& processor_name, const std::string& property_name, const std::string& property_value);
  std::string getName() const;
  utils::Identifier getComponentUUID() const;
  std::string getVersion() const;
  const core::ProcessGroup* getRoot() const { return root_.get(); }
  void setNewRoot(std::unique_ptr<core::ProcessGroup> new_root);
  void restoreBackup();
  void clearBackup();
  void stopProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                      EventDrivenSchedulingAgent& event_scheduler,
                      CronDrivenSchedulingAgent& cron_scheduler);
  void drainConnections();
  bool initialized() const {
    return root_ != nullptr;
  }

  void getConnections(std::map<std::string, core::Connectable*>& connectionMap);
  void getFlowFileContainers(std::map<std::string, core::Connectable*>& containers) const;
  bool startProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                       EventDrivenSchedulingAgent& event_scheduler,
                       CronDrivenSchedulingAgent& cron_scheduler);
  void clearConnection(const std::string &connection);

  state::StateController* getProcessorController(const std::string& id_or_name,
      const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory);

  std::optional<std::vector<state::StateController*>> getAllProcessorControllers(
          const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory);

  void setControllerSocketProtocol(c2::ControllerSocketProtocol* controller_socket_protocol) {
    controller_socket_protocol_ = controller_socket_protocol;
  }

 private:
  std::optional<std::chrono::milliseconds> loadShutdownTimeoutFromConfiguration();

  std::shared_ptr<Configure> configuration_;
  std::unique_ptr<core::ProcessGroup> root_;
  std::unique_ptr<core::ProcessGroup> backup_root_;
  state::MetricsPublisherStore* metrics_publisher_store_{};
  std::unordered_map<utils::Identifier, std::unique_ptr<state::ProcessorController>> processor_to_controller_;
  std::chrono::milliseconds shutdown_check_interval_{1000};
  c2::ControllerSocketProtocol* controller_socket_protocol_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RootProcessGroupWrapper>::getLogger();
};

}  // namespace org::apache::nifi::minifi
