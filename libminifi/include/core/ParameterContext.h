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

#include <string>
#include <unordered_set>
#include <optional>

#include "Core.h"

namespace org::apache::nifi::minifi::core {

class ParameterException : public std::runtime_error {
 public:
  explicit ParameterException(const std::string& message) : std::runtime_error(message) {
  }
};

struct Parameter {
  std::string name;
  std::string description;
  std::string value;
};

class ParameterContext : public CoreComponent {
 public:
  using CoreComponent::CoreComponent;

  void setDescription(const std::string &description) {
    description_ = description;
  }

  std::string getDescription() const {
    return description_;
  }

  void addParameter(const Parameter &parameter);
  std::optional<Parameter> getParameter(const std::string &name) const;

 private:
  std::string description_;
  std::unordered_map<std::string, Parameter> parameters_;
};

}  // namespace org::apache::nifi::minifi::core

