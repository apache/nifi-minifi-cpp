// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once


#include <memory>
#include <filesystem>

#include "FlowController.h"
#include "TestBase.h"

class TestControllerWithFlow: public TestController {
 public:
  explicit TestControllerWithFlow(const char* yamlConfigContent, bool setup_flow = true);

  void setupFlow();

  void startFlow() {
    controller_->start();
  }

  ~TestControllerWithFlow();

  std::filesystem::path home_;
  std::filesystem::path yaml_path_;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<minifi::FlowController> controller_;
  core::ProcessGroup* root_{nullptr};
  minifi::state::MetricsPublisherStore* metrics_publisher_store_{nullptr};
};
