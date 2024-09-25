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
#include <limits>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <typeinfo>
#include "minifi-cpp/core/state/Value.h"
#include "utils/ValueParser.h"
#include "utils/ValueCaster.h"
#include "utils/meta/type_list.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Purpose: Represents an AST value
 * Contains an embedded string representation to be used for a toString analog.
 *
 * Extensions can be more strongly typed and can be used anywhere where an abstract
 * representation is needed.
 */
class ValueImpl : public Value {
  using ParseException = utils::internal::ParseException;

 public:
  explicit ValueImpl(std::string value)
      : string_value(std::move(value)),
        type_id(std::type_index(typeid(std::string))) {
  }

  ~ValueImpl() override = default;

  [[nodiscard]] std::string getStringValue() const override {
    return string_value;
  }

  [[nodiscard]] const char* c_str() const override {
    return string_value.c_str();
  }

  [[nodiscard]] bool empty() const noexcept override {
    return string_value.empty();
  }

  std::type_index getTypeIndex() override {
    return type_id;
  }

  static const std::type_index UINT64_TYPE;
  static const std::type_index INT64_TYPE;
  static const std::type_index UINT32_TYPE;
  static const std::type_index INT_TYPE;
  static const std::type_index BOOL_TYPE;
  static const std::type_index DOUBLE_TYPE;
  static const std::type_index STRING_TYPE;

 protected:
  template<typename T>
  void setTypeId() {
    type_id = std::type_index(typeid(T));
  }

  bool getValue(uint32_t &ref) override {
    try {
      uint32_t value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  bool getValue(int &ref) override {
    try {
      int value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  bool getValue(int64_t &ref) override {
    try {
      int64_t value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  bool getValue(uint64_t &ref) override {
    try {
      uint64_t value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  bool getValue(bool &ref) override {
    try {
      bool value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  bool getValue(double &ref) override {
    try {
      double value;
      utils::internal::ValueParser(string_value).parse(value).parseEnd();
      ref = value;
    } catch(const ParseException&) {
      return false;
    }
    return true;
  }

  std::string string_value;
  std::type_index type_id;
};

class UInt32Value : public ValueImpl {
 public:
  explicit UInt32Value(uint32_t value)
      : ValueImpl(std::to_string(value)),
        value(value) {
    setTypeId<uint32_t>();
  }

  explicit UInt32Value(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<uint32_t>();
  }

  [[nodiscard]] uint32_t getValue() const {
    return value;
  }

 protected:
  bool getValue(uint32_t &ref) override {
    ref = value;
    return true;
  }

  bool getValue(int &ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(int64_t &ref) override {
    ref = value;
    return true;
  }

  bool getValue(uint64_t &ref) override {
    ref = value;
    return true;
  }

  bool getValue(bool& /*ref*/) override {
    return false;
  }

  bool getValue(double& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  uint32_t value{};
};

class IntValue : public ValueImpl {
 public:
  explicit IntValue(int value)
      : ValueImpl(std::to_string(value)),
        value(value) {
    setTypeId<int>();
  }

  explicit IntValue(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<int>();
  }

  [[nodiscard]] int getValue() const {
    return value;
  }

 protected:
  bool getValue(int &ref) override {
    ref = value;
    return true;
  }

  bool getValue(uint32_t &ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(int64_t &ref) override {
    ref = value;
    return true;
  }

  bool getValue(uint64_t &ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(bool& /*ref*/) override {
    return false;
  }

  bool getValue(double& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  int value{};
};

class BoolValue : public ValueImpl {
 public:
  explicit BoolValue(bool value)
      : ValueImpl(value ? "true" : "false"),
        value(value) {
    setTypeId<bool>();
  }

  explicit BoolValue(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<bool>();
  }

  [[nodiscard]] bool getValue() const {
    return value;
  }

 protected:
  bool getValue(int &ref) override {
    return PreventSwearingInFutureRefactor(ref);
  }

  bool getValue(uint32_t &ref) override {
    return PreventSwearingInFutureRefactor(ref);
  }

  bool getValue(int64_t &ref) override {
    return PreventSwearingInFutureRefactor(ref);
  }

  bool getValue(uint64_t &ref) override {
    return PreventSwearingInFutureRefactor(ref);
  }

  bool getValue(double &ref) override {
    return PreventSwearingInFutureRefactor(ref);
  }

  bool getValue(bool &ref) override {
    ref = value;
    return true;
  }

  bool value{};

 private:
  template<typename T>
  bool PreventSwearingInFutureRefactor(T &ref) {
    if (value != 0 && value != 1) {
      return false;
    }
    ref = value != 0;
    return true;
  }
};

class UInt64Value : public ValueImpl {
 public:
  explicit UInt64Value(uint64_t value)
      : ValueImpl(std::to_string(value)),
        value(value) {
    setTypeId<uint64_t>();
  }

  explicit UInt64Value(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<uint64_t>();
  }

  [[nodiscard]] uint64_t getValue() const {
    return value;
  }

 protected:
  bool getValue(int& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(uint32_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(int64_t &ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(uint64_t &ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(bool& /*ref*/) override {
    return false;
  }

  bool getValue(double& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  uint64_t value{};
};

class Int64Value : public ValueImpl {
 public:
  explicit Int64Value(int64_t value)
      : ValueImpl(std::to_string(value)),
        value(value) {
    setTypeId<int64_t>();
  }
  explicit Int64Value(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<int64_t>();
  }

  [[nodiscard]] int64_t getValue() const {
    return value;
  }

 protected:
  bool getValue(int& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(uint32_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(int64_t& ref) override {
    ref = value;
    return true;
  }

  bool getValue(uint64_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(bool& /*ref*/) override {
    return false;
  }

  bool getValue(double& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  int64_t value{};
};

class DoubleValue : public ValueImpl {
 public:
  explicit DoubleValue(double value)
      : ValueImpl(std::to_string(value)),
      value(value) {
    setTypeId<double>();
  }
  explicit DoubleValue(const std::string &strvalue)
      : ValueImpl(strvalue) {
    utils::internal::ValueParser(strvalue).parse(value).parseEnd();
    setTypeId<double>();
  }

  [[nodiscard]] double getValue() const {
    return value;
  }

 protected:
  bool getValue(int& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(uint32_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(int64_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(uint64_t& ref) override {
    return utils::internal::cast_if_in_range(value, ref);
  }

  bool getValue(bool&) override {
    return false;
  }

  bool getValue(double& ref) override {
    ref = value;
    return true;
  }

  double value{};
};

static inline std::shared_ptr<Value> createValue(const bool &object) {
  return std::make_shared<BoolValue>(object);
}

static inline std::shared_ptr<Value> createValue(const char *object) {
  return std::make_shared<ValueImpl>(object);
}

static inline std::shared_ptr<Value> createValue(char *object) {
  return std::make_shared<ValueImpl>(std::string(object));
}

static inline std::shared_ptr<Value> createValue(const std::string &object) {
  return std::make_shared<ValueImpl>(object);
}

static inline std::shared_ptr<Value> createValue(const uint32_t &object) {
  return std::make_shared<UInt32Value>(object);
}
#if ( defined(__APPLE__) || defined(__MACH__) || defined(DARWIN) )
static inline std::shared_ptr<Value> createValue(const size_t &object) {
  return std::make_shared<UInt64Value>(object);
}
#endif
static inline std::shared_ptr<Value> createValue(const uint64_t &object) {
  return std::make_shared<UInt64Value>(object);
}

static inline std::shared_ptr<Value> createValue(const int64_t &object) {
  return std::make_shared<Int64Value>(object);
}

static inline std::shared_ptr<Value> createValue(const int &object) {
  return std::make_shared<IntValue>(object);
}

static inline std::shared_ptr<Value> createValue(const double &object) {
  return std::make_shared<DoubleValue>(object);
}

template<typename T>
requires (ValueNode::supported_types::contains<T>())  // NOLINT
/* implicit, because it doesn't change the meaning, and it simplifies construction of maps */
ValueNode::ValueNode(const T value)  // NOLINT
    :value_{createValue(value)}
{}

/**
 * Define the representations and eventual storage relationships through
 * createValue
 */
template<typename T>
requires (ValueNode::supported_types::contains<T>())  // NOLINT
ValueNode& ValueNode::operator=(const T ref) {
  value_ = createValue(ref);
  return *this;
}

}  // namespace org::apache::nifi::minifi::state::response
