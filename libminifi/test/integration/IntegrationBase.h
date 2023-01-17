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

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "HTTPUtils.h"
#include "utils/FifoExecutor.h"
#include "core/state/MetricsPublisherFactory.h"
#include "c2/C2Utils.h"

namespace minifi = org::apache::nifi::minifi;
namespace core = minifi::core;
namespace utils = minifi::utils;

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

  void setKeyDir(const std::string& key_dir) {
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
      utils::file::delete_dir(state_dir);
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
  std::unique_ptr<minifi::state::response::ResponseNodeLoader> response_node_loader_;
  std::unique_ptr<minifi::FlowController> flowController_;
  std::chrono::milliseconds wait_time_;
  std::string port, scheme;
  std::string key_dir;
  std::filesystem::path state_dir;
  std::atomic<int> restart_requested_count_{0};
};

IntegrationBase::IntegrationBase(std::chrono::milliseconds waitTime)
    : configuration(std::make_shared<minifi::Configure>()),
      wait_time_(waitTime) {
}

void IntegrationBase::configureSecurity() {
  if (!key_dir.empty()) {
    configuration->set(minifi::Configure::nifi_security_client_certificate, key_dir + "cn.crt.pem");
    configuration->set(minifi::Configure::nifi_security_client_private_key, key_dir + "cn.ckey.pem");
    configuration->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir + "cn.pass");
    configuration->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir + "nifi-cert.pem");
    configuration->set(minifi::Configure::nifi_default_directory, key_dir);
  }
}

void IntegrationBase::run(const std::optional<std::filesystem::path>& test_file_location, const std::optional<std::filesystem::path>& home_path) {
  using namespace std::literals::chrono_literals;
  testSetup();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  if (test_file_location) {
    configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location->string());
  }
  configuration->set(minifi::Configure::nifi_state_storage_local_class_name, "VolatileMapStateStorage");

  configureC2();
  configureFullHeartbeat();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);

  std::atomic<bool> running = true;
  utils::FifoExecutor assertion_runner;
  std::future<void> assertions_done;
  while (running) {
    running = false;  // Stop running after this iteration, unless restart is explicitly requested

    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

    bool should_encrypt_flow_config = (configuration->get(minifi::Configure::nifi_flow_configuration_encrypt)
        | utils::flatMap(utils::StringUtils::toBool)).value_or(false);

    std::shared_ptr<utils::file::FileSystem> filesystem;
    if (home_path) {
      filesystem = std::make_shared<utils::file::FileSystem>(
          should_encrypt_flow_config,
          utils::crypto::EncryptionProvider::create(*home_path));
    } else {
      filesystem = std::make_shared<utils::file::FileSystem>();
    }

    auto flow_config = std::make_shared<core::YamlConfiguration>(core::ConfigurationContext{test_repo, content_repo, stream_factory, configuration, test_file_location, filesystem});

    auto controller_service_provider = flow_config->getControllerServiceProvider();
    char state_dir_name_template[] = "/var/tmp/integrationstate.XXXXXX";
    state_dir = utils::file::create_temp_directory(state_dir_name_template);
    if (!configuration->get(minifi::Configure::nifi_state_storage_local_path)) {
      configuration->set(minifi::Configure::nifi_state_storage_local_path, state_dir.string());
    }
    core::ProcessContext::getOrCreateDefaultStateStorage(controller_service_provider.get(), configuration);

    std::shared_ptr<core::ProcessGroup> pg(flow_config->getRoot());
    queryRootProcessGroup(pg);

    const auto request_restart = [&, this] {
      ++restart_requested_count_;
      running = true;
    };

    auto metrics_publisher_store = std::make_unique<minifi::state::MetricsPublisherStore>(configuration, test_repo, test_flow_repo, flow_config);
    flowController_ = std::make_unique<minifi::FlowController>(test_repo, test_flow_repo, configuration,
      std::move(flow_config), content_repo, std::move(metrics_publisher_store), filesystem, request_restart);
    flowController_->load();
    updateProperties(*flowController_);
    flowController_->start();

    assertions_done = assertion_runner.enqueue([this] { runAssertions(); });
    std::future_status status = std::future_status::ready;
    while (!running && (status = assertions_done.wait_for(10ms)) == std::future_status::timeout) { /* wait */ }
    if (running && status != std::future_status::timeout) {
      // cancel restart, because assertions have finished running
      running = false;
    }

    if (!running) {
      // Only stop servers if we're shutting down
      shutdownBeforeFlowController();
    }
    flowController_->stop();
  }

  cleanup();
}

struct cmd_args {
  bool isUrlSecure() const {
    // check https prefix
    return url.rfind("https://", 0) == 0;
  }

  std::string test_file;
  std::string key_dir;
  std::string bad_test_file;
  std::string url;
};

cmd_args parse_basic_cmdline_args(int argc, char ** argv) {
  cmd_args args;
  if (argc > 1) {
    args.test_file = argv[1];
  }
  if (argc > 2) {
    args.key_dir = argv[2];
  }
  return args;
}

cmd_args parse_cmdline_args(int argc, char ** argv, const std::string& uri_path = "") {
  cmd_args args = parse_basic_cmdline_args(argc, argv);
  if (argc == 2) {
    args.url = "http://localhost:0/" + uri_path;
  }
  if (argc > 2) {
    args.url = "https://localhost:0/" + uri_path;
  }
  if (argc > 3) {
    args.bad_test_file = argv[3];
  }
  return args;
}

cmd_args parse_cmdline_args_with_url(int argc, char ** argv) {
  cmd_args args = parse_basic_cmdline_args(argc, argv);
  if (argc > 3) {
    std::string url = argv[3];
#ifdef WIN32
    if (url.find("localhost") != std::string::npos) {
      std::string port, scheme, path;
      parse_http_components(url, port, scheme, path);
      url = scheme + "://" + org::apache::nifi::minifi::io::Socket::getMyHostName() + ":" + port +  path;
    }
#endif
    args.url = url;
  }
  return args;
}
