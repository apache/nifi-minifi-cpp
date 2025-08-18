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

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minifi-cpp/core/PropertyDefinition.h"
#include "PropertyValidator.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

class Property final {
 public:
  Property(std::string name, std::string description, const std::string &value, bool is_required);

  Property(std::string name, std::string description, const std::string &value);

  Property(std::string name, std::string description);

  Property(Property &&other) = default;

  Property(const Property &other) = default;

  Property();

  Property(const PropertyReference &);

  virtual ~Property() = default;

  void setTransient() { is_transient_ = true; }

  bool isTransient() const { return is_transient_; }
  std::vector<std::string> getAllowedValues() const { return allowed_values_; }
  void setAllowedValues(std::vector<std::string> allowed_values) { allowed_values_ = std::move(allowed_values); }
  std::optional<std::string> getDefaultValue() const { return default_value_; }
  std::string getName() const;
  std::string getDisplayName() const;
  std::vector<std::string> getAllowedTypes() const;
  std::string getDescription() const;
  const PropertyValidator& getValidator() const;
  [[nodiscard]] nonstd::expected<std::span<const std::string>, std::error_code> getAllValues() const;
  [[nodiscard]] nonstd::expected<std::string_view, std::error_code> getValue() const;
  nonstd::expected<void, std::error_code> setValue(std::string value);
  nonstd::expected<void, std::error_code> appendValue(std::string value);
  void clearValues();
  bool getRequired() const;
  bool isSensitive() const;
  bool supportsExpressionLanguage() const;
  std::vector<std::string> getValues();
  PropertyReference getReference() const {
    return PropertyReference(name_, display_name_, description_, is_required_, is_sensitive_, {}, {}, default_value_, validator_, supports_el_);
  }

  void setSupportsExpressionLanguage(bool supportEl);

  void addValue(const std::string &value);
  Property &operator=(const Property &other) = default;
  Property &operator=(Property &&other) = default;
  // Compare
  bool operator<(const Property &right) const;

 protected:
  std::string name_;
  std::string display_name_;
  std::string description_;
  bool is_required_;
  bool is_sensitive_ = false;
  bool is_collection_;

  std::optional<std::string> default_value_ = std::nullopt;
  std::vector<std::string> values_{};
  std::vector<std::string> allowed_values_{};
  gsl::not_null<const PropertyValidator *> validator_;

  std::vector<std::string> types_;
  bool supports_el_;
  bool is_transient_;
};

}  // namespace org::apache::nifi::minifi::core
