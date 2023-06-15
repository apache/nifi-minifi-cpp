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
#include "TypedValues.h"

namespace org::apache::nifi::minifi::core {

class PropertyValidator;

static inline std::shared_ptr<state::response::Value> convert(const std::shared_ptr<state::response::Value> &prior, const std::string &ref) {
  if (prior->getTypeIndex() == state::response::Value::UINT64_TYPE) {
    // there are specializations, so check them first
    if (std::dynamic_pointer_cast<TimePeriodValue>(prior)) {
      return std::make_shared<TimePeriodValue>(ref);
    } else if (std::dynamic_pointer_cast<DataSizeValue>(prior)) {
      return std::make_shared<DataSizeValue>(ref);
    } else {
      return std::make_shared<state::response::UInt64Value>(ref);
    }
  } else if (prior->getTypeIndex() == state::response::Value::INT64_TYPE) {
    return std::make_shared<state::response::Int64Value>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::UINT32_TYPE) {
    return std::make_shared<state::response::UInt32Value>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::INT_TYPE) {
    return std::make_shared<state::response::IntValue>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::BOOL_TYPE) {
    return std::make_shared<state::response::BoolValue>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::DOUBLE_TYPE) {
    return std::make_shared<state::response::DoubleValue>(ref);
  } else {
    return std::make_shared<state::response::Value>(ref);
  }
}

/**
 * Purpose and Design: PropertyValue extends ValueNode, bringing with it value_.
 * The reason for this is that there are additional features to support validation
 * and value translation.
 */
class PropertyValue : public state::response::ValueNode {
  using CachedValueValidator = internal::CachedValueValidator;

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
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, std::string>::value, PropertyValue&>::type {
    cached_value_validator_.invalidateCachedResult();
    return WithAssignmentGuard(ref, [&] () -> PropertyValue& {
      if (value_ == nullptr) {
        type_id = std::type_index(typeid(T));
        value_ = minifi::state::response::createValue(ref);
      } else {
        type_id = std::type_index(typeid(T));
        auto ret = convert(value_, ref);
        if (ret != nullptr) {
          value_ = ret;
        } else {
          /**
           * This is a protection mechanism that allows us to fail properties that are strictly defined.
           * To maintain backwards compatibility we allow strings to be set by way of the internal API
           * We then rely on the developer of the processor to perform the conversion. We want to get away from
           * this, so this exception will throw an exception, forcefully, when they specify types in properties.
           */
          throw utils::internal::ConversionException("Invalid conversion");
        }
      }
      return *this;
    });
  }

  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, int >::value ||
  std::is_same<T, uint32_t >::value ||
  std::is_same<T, uint64_t >::value ||
  std::is_same<T, int64_t >::value ||
  std::is_same<T, bool >::value, PropertyValue&>::type {
    cached_value_validator_.invalidateCachedResult();
    if (value_ == nullptr) {
      type_id = std::type_index(typeid(T));
      value_ = minifi::state::response::createValue(ref);
    } else {
      if (std::dynamic_pointer_cast<DataSizeValue>(value_)) {
        value_ = std::make_shared<DataSizeValue>(ref);
        type_id = DataSizeValue::type_id;
      } else if (std::dynamic_pointer_cast<TimePeriodValue>(value_)) {
        value_ = std::make_shared<TimePeriodValue>(ref);
        type_id = TimePeriodValue::type_id;
      } else if (type_id == std::type_index(typeid(T))) {
        value_ = minifi::state::response::createValue(ref);
      } else {
        // this is not the place to perform translation. There are other places within
        // the code where we can do assignments and transformations from "10" to (int)10;
        throw utils::internal::ConversionException("Assigning invalid types");
      }
    }
    return *this;
  }

  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<
  std::is_same<T, char* >::value ||
  std::is_same<T, const char* >::value, PropertyValue&>::type {
    // translated these into strings
    return operator=<std::string>(std::string(ref));
  }

  template<typename T>
  auto operator=(const std::string &ref) -> typename std::enable_if<
  std::is_same<T, DataSizeValue >::value ||
  std::is_same<T, TimePeriodValue >::value, PropertyValue&>::type {
    cached_value_validator_.invalidateCachedResult();
    return WithAssignmentGuard(ref, [&] () -> PropertyValue& {
      value_ = std::make_shared<T>(ref);
      type_id = value_->getTypeIndex();
      return *this;
    });
  }

 private:
  template<typename T>
  T convertImpl(const char* const type_name) const {
    if (!isValueUsable()) {
      throw utils::internal::InvalidValueException("Cannot convert invalid value");
    }
    T res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::internal::ConversionException(std::string("Invalid conversion to ") + type_name + " for " + value_->getStringValue());
  }

  bool isValueUsable() const {
    if (!value_) return false;
    return validate("__unknown__").valid;
  }

  template<typename Fn>
  auto WithAssignmentGuard(const std::string& ref, Fn&& functor) -> decltype(std::forward<Fn>(functor)()) {
    // TODO(adebreceni): as soon as c++17 comes jump to a RAII implementation
    // as we will need std::uncaught_exceptions()
    try {
      return std::forward<Fn>(functor)();
    } catch(const utils::internal::ValueException&) {
      type_id = std::type_index(typeid(std::string));
      value_ = minifi::state::response::createValue(ref);
      throw;
    }
  }

 protected:
  std::type_index type_id;
  CachedValueValidator cached_value_validator_;
};

inline std::string conditional_conversion(const PropertyValue &v) {
  return v.getValue()->getStringValue();
}

}  // namespace org::apache::nifi::minifi::core
