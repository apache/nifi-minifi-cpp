// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/Core.h"
#include "ControllerServiceNode.h"
#include "ControllerServiceProvider.h"
#include "core/logging/LoggerFactory.h"
#include "core/ProcessGroup.h"

namespace org::apache::nifi::minifi::core::controller {

class StandardControllerServiceNode : public ControllerServiceNode {
 public:
  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, std::shared_ptr<ControllerServiceProvider> provider, std::string id,
                                         std::shared_ptr<Configure> configuration)
      : ControllerServiceNode(std::move(service), std::move(id), std::move(configuration)),
        provider(std::move(provider)),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, std::string id, std::shared_ptr<Configure> configuration)
      : ControllerServiceNode(std::move(service), std::move(id), std::move(configuration)),
        provider(nullptr),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  StandardControllerServiceNode(const StandardControllerServiceNode &other) = delete;
  StandardControllerServiceNode &operator=(const StandardControllerServiceNode &parent) = delete;

  void initialize() override {
    ControllerServiceNode::initialize();
    active = false;
  }

  bool canEnable() override;
  bool enable() override;
  bool disable() override;

 protected:
  std::shared_ptr<ControllerServiceProvider> provider;
  std::mutex mutex_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
