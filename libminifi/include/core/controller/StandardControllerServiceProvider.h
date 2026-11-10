// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>
#include <memory>
#include <unordered_set>
#include <thread>
#include "core/controller/ControllerServiceBase.h"
#include "ControllerServiceNodeMap.h"
#include "ControllerServiceProvider.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::controller {

class StandardControllerServiceProvider : public ControllerServiceProvider  {
 public:
  explicit StandardControllerServiceProvider(std::unique_ptr<ControllerServiceNodeMap> services, std::shared_ptr<Configure> configuration, ClassLoader& loader = ClassLoader::getDefaultClassLoader())
      : ControllerServiceProvider(std::move(services)),
        extension_loader_(loader),
        configuration_(std::move(configuration)),
        admin_yield_duration_(readAdministrativeYieldDuration()),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {
  }

  StandardControllerServiceProvider(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider(StandardControllerServiceProvider &&other) = delete;

  StandardControllerServiceProvider& operator=(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider& operator=(StandardControllerServiceProvider &&other) = delete;
  ~StandardControllerServiceProvider() override {
    stopEnableRetryThread();
  }

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string& type,
      const std::string& id,
      ProcessGroup* parent_group,
      const std::optional<std::string>& alternative_key) override;
  void enableAllControllerServices() override;
  void disableAllControllerServices() override;
  void clearControllerServices() override;

 protected:
  void stopEnableRetryThread();
  void startEnableRetryThread();

  bool canEdit() override {
    return false;
  }

  ClassLoader &extension_loader_;
  std::shared_ptr<Configure> configuration_;

 private:
  std::chrono::milliseconds readAdministrativeYieldDuration() const;

  std::thread controller_service_enable_retry_thread_;
  std::atomic_bool enable_retry_thread_running_{false};
  std::mutex enable_retry_mutex_;
  std::condition_variable enable_retry_condition_;
  std::unordered_set<std::shared_ptr<ControllerServiceNode>> controller_services_to_enable_;
  std::chrono::milliseconds admin_yield_duration_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
