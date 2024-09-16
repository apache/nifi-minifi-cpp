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

#include "Configuration.h"
#include "minifi-cpp/core/AgentIdentificationProvider.h"

struct ConfigTestAccessor;

namespace org::apache::nifi::minifi {

class Configure : public virtual Configuration, public virtual core::AgentIdentificationProvider {
  friend struct ::ConfigTestAccessor;
 public:
  virtual bool get(const std::string& key, std::string& value) const = 0;
  virtual bool get(const std::string& key, const std::string& alternate_key, std::string& value) const = 0;
  virtual std::optional<std::string> get(const std::string& key) const = 0;
  virtual std::optional<std::string> getWithFallback(const std::string& key, const std::string& alternate_key) const = 0;
  virtual std::optional<std::string> getRawValue(const std::string& key) const = 0;

  virtual void setFallbackAgentIdentifier(const std::string& id) = 0;

  using Configuration::set;
  virtual void set(const std::string& key, const std::string& value, PropertyChangeLifetime lifetime) override = 0;
  virtual bool commitChanges() override = 0;

  static std::shared_ptr<Configure> create();
};

}  // namespace org::apache::nifi::minifi
