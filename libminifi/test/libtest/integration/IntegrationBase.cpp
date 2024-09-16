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
#include "IntegrationBase.h"

#include <future>

#include "utils/net/DNS.h"
#include "utils/HTTPUtils.h"
#include "unit/ProvenanceTestHelper.h"
#include "utils/FifoExecutor.h"
#include "utils/file/AssetManager.h"
#include "core/ConfigurationFactory.h"

namespace org::apache::nifi::minifi::test {

IntegrationBase::IntegrationBase(std::chrono::milliseconds waitTime)
    : configuration(std::make_shared<minifi::ConfigureImpl>()),
      wait_time_(waitTime) {
}

void IntegrationBase::configureSecurity() {
  if (!key_dir.empty()) {
    configuration->set(minifi::Configure::nifi_security_client_certificate, (key_dir / "cn.crt.pem").string());
    configuration->set(minifi::Configure::nifi_security_client_private_key, (key_dir / "cn.ckey.pem").string());
    configuration->set(minifi::Configure::nifi_security_client_pass_phrase, (key_dir / "cn.pass").string());
    configuration->set(minifi::Configure::nifi_security_client_ca_certificate, (key_dir / "nifi-cert.pem").string());
    configuration->set(minifi::Configure::nifi_default_directory, key_dir.string());
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
  minifi::utils::FifoExecutor assertion_runner;
  std::future<void> assertions_done;
  while (running) {
    running = false;  // Stop running after this iteration, unless restart is explicitly requested

    bool should_encrypt_flow_config = (configuration->get(minifi::Configure::nifi_flow_configuration_encrypt)
        | minifi::utils::andThen(minifi::utils::string::toBool)).value_or(false);

    std::shared_ptr<minifi::utils::file::FileSystem> filesystem;
    if (home_path) {
      filesystem = std::make_shared<minifi::utils::file::FileSystem>(
          should_encrypt_flow_config,
          minifi::utils::crypto::EncryptionProvider::create(*home_path));
    } else {
      filesystem = std::make_shared<minifi::utils::file::FileSystem>();
    }

    std::optional<minifi::utils::crypto::EncryptionProvider> sensitive_values_encryptor = [&]() {
      if (home_path) {
        return minifi::utils::crypto::EncryptionProvider::createSensitivePropertiesEncryptor(*home_path);
      } else {
        auto encryption_key = minifi::utils::string::from_hex("e4bce4be67f417ed2530038626da57da7725ff8c0b519b692e4311e4d4fe8a28");
        return minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{encryption_key}};
      }
    }();

    std::string nifi_configuration_class_name = "adaptiveconfiguration";
    configuration->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

    std::shared_ptr<core::FlowConfiguration> flow_config = core::createFlowConfiguration(
        core::ConfigurationContext{
            .flow_file_repo = test_repo,
            .content_repo = content_repo,
            .configuration = configuration,
            .path = test_file_location,
            .filesystem = filesystem,
            .sensitive_values_encryptor = sensitive_values_encryptor
        }, nifi_configuration_class_name);

    auto controller_service_provider = flow_config->getControllerServiceProvider();
    char state_dir_name_template[] = "/var/tmp/integrationstate.XXXXXX";  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    state_dir = minifi::utils::file::create_temp_directory(state_dir_name_template);
    if (!configuration->get(minifi::Configure::nifi_state_storage_local_path)) {
      configuration->set(minifi::Configure::nifi_state_storage_local_path, state_dir.string());
    }
    core::ProcessContextImpl::getOrCreateDefaultStateStorage(controller_service_provider.get(), configuration);

    std::shared_ptr<core::ProcessGroup> pg(flow_config->getRoot());
    queryRootProcessGroup(pg);

    const auto request_restart = [&, this] {
      ++restart_requested_count_;
      running = true;
    };

    std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repo_metric_sources{test_repo, test_flow_repo, content_repo};
    asset_manager_ = std::make_unique<minifi::utils::file::AssetManager>(*configuration);
    auto metrics_publisher_store = std::make_unique<minifi::state::MetricsPublisherStore>(configuration, repo_metric_sources, flow_config, asset_manager_.get());
    flowController_ = std::make_unique<minifi::FlowController>(test_repo, test_flow_repo, configuration,
      std::move(flow_config), content_repo, std::move(metrics_publisher_store), filesystem, request_restart, asset_manager_.get());
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

std::string parseUrl(std::string url) {
#ifdef WIN32
  if (url.find("localhost") != std::string::npos) {
    std::string port, scheme, path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    url = scheme + "://" + org::apache::nifi::minifi::utils::net::getMyHostName() + ":" + port +  path;
  }
#endif
  return url;
}

}  // namespace org::apache::nifi::minifi::test
