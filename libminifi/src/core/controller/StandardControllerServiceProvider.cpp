// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/controller/StandardControllerServiceProvider.h"

#include "core/controller/StandardControllerServiceNode.h"
#include "core/TypedValues.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::controller {

std::shared_ptr<ControllerServiceNode> StandardControllerServiceProvider::createControllerService(const std::string& type,
    const std::string& id,
    ProcessGroup* parent_group,
    const std::optional<std::string>& alternative_key) {
  std::shared_ptr<ControllerService> new_controller_service = extension_loader_.instantiate<ControllerService>(type, id);

  if (!new_controller_service) {
    return nullptr;
  }

  std::shared_ptr<ControllerServiceNode> new_service_node = std::make_shared<StandardControllerServiceNode>(new_controller_service,
                                                                                                            sharedFromThis<ControllerServiceProvider>(), id,
                                                                                                            configuration_);

  controller_map_->put(id, new_service_node, parent_group);
  if (alternative_key) {
    controller_map_->registerAlternativeKey(id, *alternative_key);
  }

  return new_service_node;
}

void StandardControllerServiceProvider::enableAllControllerServices() {
  gsl_Expects(!enable_retry_thread_running_);
  {
    std::lock_guard<std::mutex> lock(enable_retry_mutex_);
    logger_->log_info("Enabling {} controller services", controller_map_->getAllControllerServices().size());
    for (const auto& service : controller_map_->getAllControllerServices()) {
      logger_->log_info("Enabling {}", service->getName());
      if (!service->canEnable()) {
        logger_->log_warn("Service {} cannot be enabled", service->getName());
        continue;
      }
      if (!service->enable()) {
        logger_->log_warn("Could not enable {}", service->getName());
        controller_services_to_enable_.insert(service);
      }
    }
  }
  startEnableRetryThread();
}

void StandardControllerServiceProvider::disableAllControllerServices() {
  stopEnableRetryThread();
  logger_->log_info("Disabling {} controller services", controller_map_->getAllControllerServices().size());
  for (const auto& service : controller_map_->getAllControllerServices()) {
    logger_->log_info("Disabling {}", service->getName());
    if (!service->disable()) {
      logger_->log_warn("Could not disable {}", service->getName());
    }
  }
}

void StandardControllerServiceProvider::clearControllerServices() {
  stopEnableRetryThread();
  controller_map_->clear();
}

void StandardControllerServiceProvider::stopEnableRetryThread() {
  enable_retry_thread_running_ = false;
  enable_retry_condition_.notify_all();
  if (controller_service_enable_retry_thread_.joinable()) {
    controller_service_enable_retry_thread_.join();
  }
}

void StandardControllerServiceProvider::startEnableRetryThread() {
  enable_retry_thread_running_ = true;
  controller_service_enable_retry_thread_ = std::thread([this]() {
    if (controller_services_to_enable_.empty()) {
      return;
    }
    std::unique_lock<std::mutex> lock(enable_retry_mutex_);
    enable_retry_condition_.wait_for(lock, admin_yield_duration_, [this]() {
      return !enable_retry_thread_running_;
    });
    while (enable_retry_thread_running_) {
      for (auto it = controller_services_to_enable_.begin(); it != controller_services_to_enable_.end();) {
        if ((*it)->enable()) {
          it = controller_services_to_enable_.erase(it);
        } else {
          ++it;
        }
      }
      if (controller_services_to_enable_.empty()) {
        break;
      }
      enable_retry_condition_.wait_for(lock, admin_yield_duration_, [this]() {
        return !enable_retry_thread_running_;
      });
    }
    controller_services_to_enable_.clear();
  });
}

std::chrono::milliseconds StandardControllerServiceProvider::readAdministrativeYieldDuration() const {
  std::chrono::milliseconds admin_yield_duration = 30s;
  std::string yield_value_str;

  if (configuration_->get(Configure::nifi_administrative_yield_duration, yield_value_str)) {
    std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yield_value_str);
    if (value) {
      admin_yield_duration = value->getMilliseconds();
    }
  }
  return admin_yield_duration;
}

}  // namespace org::apache::nifi::minifi::core::controller
