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

#include "properties/Configure.h"

#include "utils/gsl.h"

#include "core/logging/LoggerConfiguration.h"
#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"
#include "properties/Configuration.h"

namespace org::apache::nifi::minifi {

bool ConfigureImpl::get(const std::string& key, std::string& value) const {
  if (auto opt_value = getRawValue(key)) {
    value = *opt_value;
    if (decryptor_ && isEncrypted(key)) {
      value = decryptor_->decrypt(value);
    }
    return true;
  }
  return false;
}

bool ConfigureImpl::get(const std::string& key, const std::string& alternate_key, std::string& value) const {
  if (get(key, value)) {
    if (get(alternate_key)) {
      const auto logger = core::logging::LoggerFactory<Configure>::getLogger();
      logger->log_warn("Both the property '{}' and an alternate property '{}' are set. Using '{}'.", key, alternate_key, key);
    }
    return true;
  } else if (get(alternate_key, value)) {
    const auto logger = core::logging::LoggerFactory<Configure>::getLogger();
    logger->log_warn("{} is an alternate property that may not be supported in future releases. Please use {} instead.", alternate_key, key);
    return true;
  } else {
    return false;
  }
}

std::optional<std::string> ConfigureImpl::get(const std::string& key) const {
  std::string value;
  if (get(key, value)) {
    return value;
  }
  return std::nullopt;
}

std::optional<std::string> ConfigureImpl::getWithFallback(const std::string& key, const std::string& alternate_key) const {
  std::string value;
  if (get(key, alternate_key, value)) {
    return value;
  }
  return std::nullopt;
}

std::optional<std::string> ConfigureImpl::getRawValue(const std::string& key) const {
  static constexpr std::string_view log_prefix = "nifi.log.";
  if (utils::string::startsWith(key, log_prefix)) {
    if (logger_properties_) {
      return logger_properties_->getString(key.substr(log_prefix.length()));
    }
    return std::nullopt;
  }

  return getString(key);
}

bool ConfigureImpl::isEncrypted(const std::string& key) const {
  gsl_Expects(decryptor_);
  const auto encryption_marker = getString(key + ".protected");
  return decryptor_->isValidEncryptionMarker(encryption_marker);
}

std::optional<std::string> ConfigureImpl::getAgentClass() const {
  std::string agent_class;
  if (get(Configuration::nifi_c2_agent_class, "c2.agent.class", agent_class) && !agent_class.empty()) {
    return agent_class;
  }
  return {};
}

std::string ConfigureImpl::getAgentIdentifier() const {
  std::string agent_id;
  if (!get(Configuration::nifi_c2_agent_identifier, "c2.agent.identifier", agent_id) || agent_id.empty()) {
    std::lock_guard<std::mutex> guard(fallback_identifier_mutex_);
    return fallback_identifier_;
  }
  return agent_id;
}

void ConfigureImpl::setFallbackAgentIdentifier(const std::string& id) {
  std::lock_guard<std::mutex> guard(fallback_identifier_mutex_);
  fallback_identifier_ = id;
}

void ConfigureImpl::set(const std::string& key, const std::string& value, PropertyChangeLifetime lifetime) {
  const std::string_view log_prefix = "nifi.log.";
  if (utils::string::startsWith(key, log_prefix)) {
    if (logger_properties_) {
      logger_properties_changed_ = true;
      logger_properties_->set(key.substr(log_prefix.length()), value, lifetime);
    }
  } else {
    ConfigurationImpl::set(key, value, lifetime);
  }
}

bool ConfigureImpl::commitChanges() {
  bool success = true;
  if (logger_properties_) {
    success &= logger_properties_->commitChanges();
    if (logger_properties_changed_) {
      core::logging::LoggerConfiguration::getConfiguration().initialize(logger_properties_);
      logger_properties_changed_ = false;
    }
  }
  success &= ConfigurationImpl::commitChanges();
  return success;
}

std::shared_ptr<Configure> Configure::create() {
  return std::make_shared<ConfigureImpl>();
}

}  // namespace org::apache::nifi::minifi
