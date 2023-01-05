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

#include <string>
#include <utility>

namespace org::apache::nifi::minifi::core {

class DynamicProperty {
 public:
  DynamicProperty() = default;  // required by VS 2019 to create an empty array; not required by VS 2022

  DynamicProperty(std::string name, std::string value, std::string description, bool supports_expression_language)
      : name_(std::move(name)),
        value_(std::move(value)),
        description_(std::move(description)),
        supports_expression_language_(supports_expression_language) {
  }

  [[nodiscard]] std::string getName() const {
    return name_;
  }

  [[nodiscard]] std::string getValue() const {
    return value_;
  }

  [[nodiscard]] std::string getDescription() const {
    return description_;
  }

  [[nodiscard]] bool supportsExpressionLanguage() const {
    return supports_expression_language_;
  }

 private:
  std::string name_;
  std::string value_;
  std::string description_;
  bool supports_expression_language_ = false;
};

}  // namespace org::apache::nifi::minifi::core
