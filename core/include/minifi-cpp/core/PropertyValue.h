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

#include <typeindex>
#include <string>
#include <utility>
#include <memory>

#include "CachedValueValidator.h"
#include "ValidationResult.h"
#include "state/Value.h"
#include "minifi-cpp/utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {

class PropertyValidator;

/**
 * Purpose and Design: PropertyValue extends ValueNode, bringing with it value_.
 * The reason for this is that there are additional features to support validation
 * and value translation.
 */
class PropertyValue : public state::response::ValueNode {
  using CachedValueValidator = internal::CachedValueValidator;
  template<typename T, typename> friend struct Converter;

 public:
  PropertyValue()
      : type_id(std::type_index(typeid(std::string))) {}

  PropertyValue(const PropertyValue &o) = default;
  PropertyValue(PropertyValue &&o) noexcept = default;

  template<typename T>
  static PropertyValue parse(std::string_view input, const PropertyValidator& validator) {
    PropertyValue property_value;
    property_value.value_ = std::make_shared<T>(std::string{input});
    property_value.type_id = property_value.value_->getTypeIndex();
    property_value.cached_value_validator_ = validator;
    return property_value;
  }

  void setValidator(const PropertyValidator& validator) {
    cached_value_validator_ = validator;
  }

  ValidationResult validate(const std::string &subject) const {
    return cached_value_validator_.validate(subject, getValue());
  }

  operator uint64_t() const {
    return convertImpl<uint64_t>("uint64_t");
  }

  operator int64_t() const {
    return convertImpl<int64_t>("int64_t");
  }

  operator uint32_t() const {
    return convertImpl<uint32_t>("uint32_t");
  }

  operator int() const {
    return convertImpl<int>("int");
  }

  operator bool() const {
    return convertImpl<bool>("bool");
  }

  operator double() const {
    return convertImpl<double>("double");
  }

  const char* c_str() const {
    if (!isValueUsable()) {
      throw utils::internal::InvalidValueException("Cannot convert invalid value");
    }
    return value_ ? value_->c_str() : "";
  }

  operator std::string() const {
    if (!isValueUsable()) {
      throw utils::internal::InvalidValueException("Cannot convert invalid value");
    }
    return to_string();
  }

  PropertyValue &operator=(PropertyValue &&o) = default;
  PropertyValue &operator=(const PropertyValue &o) = default;

  std::type_index getTypeInfo() const {
    return type_id;
  }
  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  auto operator=(const T& ref) -> std::enable_if_t<!std::is_same_v<std::decay_t<T>, PropertyValue>, PropertyValue&>;

 private:
  template<typename T>
  T convertImpl(const char* const type_name) const;

  bool isValueUsable() const {
    if (!value_) return false;
    return validate("__unknown__").valid;
  }

  template<typename Fn>
  auto WithAssignmentGuard(const std::string& ref, Fn&& functor) -> decltype(std::forward<Fn>(functor)());

 protected:
  std::type_index type_id;
  CachedValueValidator cached_value_validator_;
};

inline std::string conditional_conversion(const PropertyValue &v) {
  return v.getValue()->getStringValue();
}

}  // namespace org::apache::nifi::minifi::core
