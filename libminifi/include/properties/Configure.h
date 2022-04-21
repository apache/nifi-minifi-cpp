/**
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

#include <optional>
#include <string>
#include <utility>
#include <memory>

#include "properties/Configuration.h"
#include "properties/Decryptor.h"
#include "core/AgentIdentificationProvider.h"
#include "core/logging/LoggerProperties.h"

struct ConfigTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Configure : public Configuration, public core::AgentIdentificationProvider {
  friend struct ::ConfigTestAccessor;
 public:
  explicit Configure(std::optional<Decryptor> decryptor = std::nullopt, std::shared_ptr<core::logging::LoggerProperties> logger_properties = {})
      : Configuration{}, decryptor_(std::move(decryptor)), logger_properties_(std::move(logger_properties)) {}

  bool get(const std::string& key, std::string& value) const;
  bool get(const std::string& key, const std::string& alternate_key, std::string& value) const;
  std::optional<std::string> get(const std::string& key) const;
  std::optional<std::string> getRawValue(const std::string& key) const;

  std::optional<std::string> getAgentClass() const override;
  std::string getAgentIdentifier() const override;
  void setFallbackAgentIdentifier(const std::string& id);

  using Configuration::set;
  void set(const std::string& key, const std::string& value, PropertyChangeLifetime lifetime) override;
  bool commitChanges() override;


 private:
  // WARNING! a test utility
  void setLoggerProperties(std::shared_ptr<core::logging::LoggerProperties> new_properties) {
    logger_properties_ = new_properties;
  }

  bool isEncrypted(const std::string& key) const;

  std::optional<Decryptor> decryptor_;
  mutable std::mutex fallback_identifier_mutex_;
  std::string fallback_identifier_;
  std::atomic_bool logger_properties_changed_{false};
  std::shared_ptr<core::logging::LoggerProperties> logger_properties_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
