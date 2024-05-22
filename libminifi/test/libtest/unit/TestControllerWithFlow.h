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
