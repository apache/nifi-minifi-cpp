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

#include "minifi-cpp/core/PropertyValue.h"

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
    } else if (std::dynamic_pointer_cast<DataTransferSpeedValue>(prior)) {
      return std::make_shared<DataTransferSpeedValue>(ref);
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
    return std::make_shared<state::response::ValueImpl>(ref);
  }
}

template<typename Fn>
auto PropertyValue::WithAssignmentGuard( const std::string& ref, Fn&& functor) -> decltype(std::forward<Fn>(functor)()) {
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

template<typename T, typename = void>
struct Converter;

template<typename T>
struct Converter<T, std::enable_if_t<std::is_same_v<T, std::string>, void>> {
  void operator()(PropertyValue& self, const T& ref) {
    self.cached_value_validator_.invalidateCachedResult();
    self.WithAssignmentGuard(ref, [&] () {
      if (self.value_ == nullptr) {
        self.type_id = std::type_index(typeid(T));
        self.value_ = minifi::state::response::createValue(ref);
      } else {
        self.type_id = std::type_index(typeid(T));
        auto ret = convert(self.value_, ref);
        if (ret != nullptr) {
          self.value_ = ret;
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
    });
  }
};

template<typename T>
struct Converter<T, std::enable_if_t<std::is_same_v<T, int > || std::is_same_v<T, uint32_t > || std::is_same_v<T, uint64_t > || std::is_same_v<T, int64_t > || std::is_same_v<T, bool >, void>> {
  void operator()(PropertyValue& self, const T& ref) {
    self.cached_value_validator_.invalidateCachedResult();
    if (self.value_ == nullptr) {
      self.type_id = std::type_index(typeid(T));
      self.value_ = minifi::state::response::createValue(ref);
    } else {
      if (std::dynamic_pointer_cast<DataSizeValue>(self.value_)) {
        self.value_ = std::make_shared<DataSizeValue>(ref);
        self.type_id = DataSizeValue::type_id;
      } else if (std::dynamic_pointer_cast<DataTransferSpeedValue>(self.value_)) {
        self.value_ = std::make_shared<DataTransferSpeedValue>(ref);
        self.type_id = DataTransferSpeedValue::type_id;
      } else if (std::dynamic_pointer_cast<TimePeriodValue>(self.value_)) {
        self.value_ = std::make_shared<TimePeriodValue>(ref);
        self.type_id = TimePeriodValue::type_id;
      } else if (self.type_id == std::type_index(typeid(T))) {
        self.value_ = minifi::state::response::createValue(ref);
      } else {
        // this is not the place to perform translation. There are other places within
        // the code where we can do assignments and transformations from "10" to (int)10;
        throw utils::internal::ConversionException("Assigning invalid types");
      }
    }
  }
};

template<typename T>
struct Converter<T, std::enable_if_t<std::is_same_v<T, char* > || std::is_same_v<T, const char* >, void>> {
  void operator()(PropertyValue& self, const T& ref) {
    // translated these into strings
    self.operator=<std::string>(std::string(ref));
  }
};

template<typename T>
auto PropertyValue::operator=(const T& ref) -> std::enable_if_t<!std::is_same_v<std::decay_t<T>, PropertyValue>, PropertyValue&> {
  Converter<T>{}(*this, ref);
  return *this;
}

template<typename T>
T PropertyValue::convertImpl(const char* const type_name) const {
  if (!isValueUsable()) {
    throw utils::internal::InvalidValueException("Cannot convert invalid value");
  }
  T res;
  if (auto* value_impl = dynamic_cast<state::response::ValueImpl*>(value_.get())) {
    if (value_impl->convertValue(res)) {
      return res;
    }
  }
  throw utils::internal::ConversionException(std::string("Invalid conversion to ") + type_name + " for " + value_->getStringValue());
}

}  // namespace org::apache::nifi::minifi::core
