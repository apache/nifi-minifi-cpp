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

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/Core.h"
#include "PropertyValue.h"
#include "minifi-cpp/core/state/Value.h"
#include "ValidationResult.h"

namespace org::apache::nifi::minifi::core {

class PropertyParser {
 public:
  virtual constexpr ~PropertyParser() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413

  [[nodiscard]] virtual PropertyValue parse(std::string_view input) const = 0;
};

class PropertyValidator {
 public:
  virtual constexpr ~PropertyValidator() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413

  [[nodiscard]] virtual std::string_view getValidatorName() const = 0;

  [[nodiscard]] virtual ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const = 0;

  [[nodiscard]] virtual ValidationResult validate(const std::string &subject, const std::string &input) const = 0;
};

class PropertyType : public PropertyParser, public PropertyValidator {
 public:
  virtual constexpr ~PropertyType() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413
};

}  // namespace org::apache::nifi::minifi::core
