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

#define DEFAULT_WAITTIME_MSECS 10000

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "utils/file/AssetManager.h"
#include "utils/file/FileUtils.h"

namespace minifi = org::apache::nifi::minifi;
namespace core = minifi::core;
namespace utils = minifi::utils;

namespace org::apache::nifi::minifi::test {

class IntegrationBase {
 public:
  explicit IntegrationBase(std::chrono::milliseconds waitTime = std::chrono::milliseconds(DEFAULT_WAITTIME_MSECS));
  IntegrationBase(const IntegrationBase&) = delete;
  IntegrationBase(IntegrationBase&& other) noexcept
      :configuration{std::move(other.configuration)},
      flowController_{std::move(other.flowController_)},
      wait_time_{other.wait_time_},
      port{std::move(other.port)},
      scheme{std::move(other.scheme)},
      key_dir{std::move(other.key_dir)},
      state_dir{std::move(other.state_dir)},
      restart_requested_count_{other.restart_requested_count_.load()}
  {}
  IntegrationBase& operator=(const IntegrationBase&) = delete;
  IntegrationBase& operator=(IntegrationBase&& other) noexcept {
    if (&other == this) return *this;
    configuration = std::move(other.configuration);
    flowController_ = std::move(other.flowController_);
    wait_time_ = other.wait_time_;
    port = std::move(other.port);
    scheme = std::move(other.scheme);
    key_dir = std::move(other.key_dir);
    state_dir = std::move(other.state_dir);
    restart_requested_count_ = other.restart_requested_count_.load();
    return *this;
  }
  virtual ~IntegrationBase() = default;

  virtual void run(const std::optional<std::filesystem::path>& test_file_location = {}, const std::optional<std::filesystem::path>& home_path = {});

  void setKeyDir(const std::filesystem::path& key_dir) {
    this->key_dir = key_dir;
    configureSecurity();
  }

  virtual void testSetup() = 0;

  virtual void shutdownBeforeFlowController() {
  }

  const std::shared_ptr<minifi::Configure>& getConfiguration() const {
    return configuration;
  }

  void setConfiguration(std::shared_ptr<minifi::Configure> configuration) {
    this->configuration = std::move(configuration);
  }

  virtual void cleanup() {
    if (!state_dir.empty()) {
      minifi::utils::file::delete_dir(state_dir);
    }
  }

  virtual void runAssertions() = 0;

 protected:
  virtual void configureC2() {
  }

  virtual void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> /*pg*/) {
  }

  virtual void configureFullHeartbeat() {
  }

  virtual void updateProperties(minifi::FlowController& /*fc*/) {
  }

  void configureSecurity();
  std::shared_ptr<minifi::Configure> configuration;
  std::unique_ptr<minifi::utils::file::AssetManager> asset_manager_;
  std::unique_ptr<minifi::state::response::ResponseNodeLoader> response_node_loader_;
  std::unique_ptr<minifi::FlowController> flowController_;
  std::chrono::milliseconds wait_time_;
  std::string port, scheme;
  std::filesystem::path key_dir;
  std::filesystem::path state_dir;
  std::atomic<int> restart_requested_count_{0};
};

std::string parseUrl(std::string url);

}  // namespace org::apache::nifi::minifi::test
