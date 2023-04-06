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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <string_view>

#include "Property.h"
#include "PropertyValidation.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

template<typename T>
class ConstrainedProperty;

class PropertyBuilder : public std::enable_shared_from_this<PropertyBuilder> {
 public:
  static std::shared_ptr<PropertyBuilder> createProperty(const std::string &name) {
    std::shared_ptr<PropertyBuilder> builder = std::unique_ptr<PropertyBuilder>(new PropertyBuilder());
    builder->prop.name_ = name;
    return builder;
  }

  static std::shared_ptr<PropertyBuilder> createProperty(const std::string &name, const std::string &displayName) {
    std::shared_ptr<PropertyBuilder> builder = std::unique_ptr<PropertyBuilder>(new PropertyBuilder());
    builder->prop.name_ = name;
    builder->prop.display_name_ = displayName;
    return builder;
  }

  std::shared_ptr<PropertyBuilder> withDescription(const std::string &description) {
    prop.description_ = description;
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> isRequired(bool required) {
    prop.is_required_ = required;
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> supportsExpressionLanguage(bool sel) {
    prop.supports_el_ = sel;
    return shared_from_this();
  }

  template<typename T>
  std::shared_ptr<PropertyBuilder> withDefaultValue(const T &df) {
    prop.default_value_ = df;

    prop.validator_ = StandardValidators::getValidator(prop.default_value_.getValue());
    prop.default_value_.setValidator(*prop.validator_);

    // inspect the type and add a validator to this.
    // there may be cases in which the validator is typed differently
    // and a validator can be added for this.
    return shared_from_this();
  }

  template<typename T>
  std::shared_ptr<PropertyBuilder> withDefaultValue(const T &df, const PropertyValidator& validator) {
    prop.default_value_ = df;

    prop.default_value_.setValidator(validator);
    prop.validator_ = gsl::make_not_null(&validator);

    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> withType(const PropertyValidator& validator) {
    prop.validator_ = gsl::make_not_null(&validator);
    prop.default_value_.setValidator(validator);
    return shared_from_this();
  }

  template<typename T>
  std::shared_ptr<ConstrainedProperty<T>> withAllowableValue(const T& df) {
    auto property = std::make_shared<ConstrainedProperty<T>>(shared_from_this());
    property->withAllowableValue(df);
    return property;
  }

  template<typename T>
  std::shared_ptr<ConstrainedProperty<T>> withAllowableValues(const std::set<T> &df) {
    auto property = std::make_shared<ConstrainedProperty<T>>(shared_from_this());
    property->withAllowableValues(df);
    return property;
  }

  template<typename T>
  std::shared_ptr<PropertyBuilder> withDefaultValue(std::string_view df) {
    prop.default_value_.operator=<T>(std::string{df});

    prop.validator_ = StandardValidators::getValidator(prop.default_value_.getValue());
    prop.default_value_.setValidator(*prop.validator_);

    // inspect the type and add a validator to this.
    // there may be cases in which the validator is typed differently
    // and a validator can be added for this.
    return shared_from_this();
  }

  template<typename T>
  std::shared_ptr<PropertyBuilder> asType() {
    prop.types_.push_back(core::getClassName<T>());
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> withExclusiveProperty(const std::string &property, const std::string regex) {
    prop.exclusive_of_properties_.push_back({ property, regex });
    return shared_from_this();
  }

  Property &&build() {
    return std::move(prop);
  }

 private:
  Property prop;

  PropertyBuilder() = default;
};

template<typename T>
class ConstrainedProperty : public std::enable_shared_from_this<ConstrainedProperty<T>> {
 public:
  std::shared_ptr<ConstrainedProperty<T>> withDescription(const std::string &description) {
    builder_->withDescription(description);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> isRequired(bool required) {
    builder_->isRequired(required);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> supportsExpressionLanguage(bool sel) {
    builder_->supportsExpressionLanguage(sel);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> withDefaultValue(const T &df) {
    builder_->withDefaultValue(df);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> withDefaultValue(const T &df, const PropertyValidator& validator) {
    builder_->withDefaultValue(df, validator);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> withAllowableValue(const T& df) {
    PropertyValue dn;
    dn = df;
    allowed_values_.emplace_back(dn);
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> withAllowableValues(const std::set<T>& defaultValues) {
    for (const auto &defaultValue : defaultValues) {
      PropertyValue dn;
      dn = defaultValue;
      allowed_values_.emplace_back(dn);
    }
    return this->shared_from_this();
  }

  template<typename J>
  std::shared_ptr<ConstrainedProperty<T>> asType() {
    builder_->asType<J>();
    return this->shared_from_this();
  }

  std::shared_ptr<ConstrainedProperty<T>> withExclusiveProperty(const std::string &property, const std::string regex) {
    builder_->withExclusiveProperty(property, regex);
    return this->shared_from_this();
  }

  Property &&build() {
    Property &&prop = builder_->build();
    for (const auto &value : allowed_values_) {
      prop.addAllowedValue(value);
    }
    return std::move(prop);
  }

  ConstrainedProperty(const std::shared_ptr<PropertyBuilder> &builder) // NOLINT
      : builder_(builder) {
  }

 protected:
  std::vector<PropertyValue> allowed_values_;
  std::shared_ptr<PropertyBuilder> builder_;

  friend class PropertyBuilder;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
