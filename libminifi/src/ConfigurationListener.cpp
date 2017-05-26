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
#include "ConfigurationListener.h"
#include "FlowController.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include <memory>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void ConfigurationListener::start() {
  if (running_)
    return;

  pull_interval_ = 60 * 1000;
  std::string value;
  // grab the value for configuration
  if (configure_->get(Configure::nifi_configuration_listener_pull_interval,
      value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, pull_interval_, unit)
        && core::Property::ConvertTimeUnitToMS(pull_interval_, unit,
            pull_interval_)) {
      logger_->log_info("Configuration Listener pull interval: [%d] ms",
           pull_interval_);
    }
  }

  std::string clientAuthStr;
  if (configure_->get(Configure::nifi_configuration_listener_need_ClientAuth, clientAuthStr)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(clientAuthStr, this->need_client_certificate_);
  }

  if (configure_->get(
          Configure::nifi_configuration_listener_client_ca_certificate,
      this->ca_certificate_)) {
    logger_->log_info("Configuration Listener CA certificates: [%s]",
        this->ca_certificate_.c_str());
  }

  if (this->need_client_certificate_) {
    std::string passphrase_file;

    if (!(configure_->get(
        Configure::nifi_configuration_listener_client_certificate, this->certificate_)
        && configure_->get(Configure::nifi_configuration_listener_private_key,
            this->private_key_))) {
      logger_->log_error(
          "Certificate and Private Key PEM file not configured for configuration listener, error: %s.",
          std::strerror(errno));
    }

    if (configure_->get(
        Configure::nifi_configuration_listener_client_pass_phrase,
        passphrase_file)) {
      // load the passphase from file
      std::ifstream file(passphrase_file.c_str(), std::ifstream::in);
      if (file.good()) {
        this->passphrase_.assign((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        file.close();
      }
    }

    logger_->log_info("Configuration Listener certificate: [%s], private key: [%s], passphrase file: [%s]",
            this->certificate_.c_str(), this->private_key_.c_str(), passphrase_file.c_str());
  }

  thread_ = std::thread(&ConfigurationListener::threadExecutor, this);
  thread_.detach();
  running_ = true;
  logger_->log_info("%s ConfigurationListener Thread Start", type_.c_str());
}

void ConfigurationListener::stop() {
  if (!running_)
    return;
  running_ = false;
  if (thread_.joinable())
    thread_.join();
  logger_->log_info("%s ConfigurationListener Thread Stop", type_.c_str());
}

void ConfigurationListener::run() {
  int64_t interval = 0;
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    interval += 100;
    if (interval >= pull_interval_) {
      std::string payload;
      bool ret = false;
      ret = pullConfiguration(payload);
      if (ret) {
        if (payload.empty() || payload == lastAppliedConfiguration) {
          interval = 0;
          continue;
        }
        ret = this->controller_->applyConfiguration(payload);
        if (ret)
          this->lastAppliedConfiguration = payload;
      }
      interval = 0;
    }
  }
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
