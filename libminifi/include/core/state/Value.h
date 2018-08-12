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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_VALUE_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_VALUE_H_

#include <typeindex>
#include <limits>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <typeinfo>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Purpose: Represents an AST value
 * Contains an embedded string representation to be used for a toString analog.
 *
 * Extensions can be more strongly typed and can be used anywhere where an abstract
 * representation is needed.
 */
class Value {
 public:

  explicit Value(const std::string &value)
      : string_value(value),
        type_id(std::type_index(typeid(std::string))) {
  }

  virtual ~Value() {

  }
  std::string getStringValue() const {
    return string_value;
  }

  template<typename T>
  bool convertValue(T &ref) {
    return convertValueImpl<typename std::common_type<T>::type>(ref);
  }

  bool empty() {
    return string_value.empty();
  }

  std::type_index getTypeIndex() {
    return type_id;
  }

  static const std::type_index UINT64_TYPE;
  static const std::type_index INT64_TYPE;
  static const std::type_index INT_TYPE;
  static const std::type_index BOOL_TYPE;
  static const std::type_index STRING_TYPE;

 protected:

  template<typename T>
  bool convertValueImpl(T &ref) {
    return getValue(ref);
  }

  template<typename T>
  void setTypeId() {
    type_id = std::type_index(typeid(T));
  }

  virtual bool getValue(int &ref) {
    ref = std::stol(string_value);
    return true;
  }

  virtual bool getValue(int64_t &ref) {
    ref = std::stoll(string_value);
    return true;
  }

  virtual bool getValue(uint64_t &ref) {
    ref = std::stoull(string_value);
    return true;
  }

  virtual bool getValue(bool &ref) {
    std::istringstream(string_value) >> std::boolalpha >> ref;
    return true;
  }

  std::string string_value;
  std::type_index type_id;
};

class IntValue : public Value {
 public:
  explicit IntValue(int value)
      : Value(std::to_string(value)),
        value(value) {
    setTypeId<int>();
  }

  explicit IntValue(const std::string &strvalue)
      : Value(strvalue),
        value(std::stoi(strvalue)) {

  }
  int getValue() const {
    return value;
  }

 protected:

  virtual bool getValue(int &ref) {
    ref = value;
    return true;
  }

  virtual bool getValue(int64_t &ref) {
    ref = value;
    return true;
  }

  virtual bool getValue(uint64_t &ref) {
    ref = value;
    return true;
  }

  virtual bool getValue(bool &ref) {
    return false;
  }

  int value;
};

class BoolValue : public Value {
 public:
  explicit BoolValue(bool value)
      : Value(value ? "true" : "false"),
        value(value) {
    setTypeId<bool>();
  }

  explicit BoolValue(const std::string &strvalue)
      : Value(strvalue) {
    bool l;
    std::istringstream(strvalue) >> std::boolalpha >> l;
    value = l;  // avoid warnings
  }

  bool getValue() const {
    return value;
  }
 protected:

  virtual bool getValue(int &ref) {
    if (ref == 1) {
      ref = true;
      return true;
    } else if (ref == 0) {
      ref = false;
      return true;
    } else {
      return false;
    }
  }

  virtual bool getValue(int64_t &ref) {
    if (ref == 1) {
      ref = true;
      return true;
    } else if (ref == 0) {
      ref = false;
      return true;
    } else {
      return false;
    }
  }

  virtual bool getValue(uint64_t &ref) {
    if (ref == 1) {
      ref = true;
      return true;
    } else if (ref == 0) {
      ref = false;
      return true;
    } else {
      return false;
    }
  }

  virtual bool getValue(bool &ref) {
    ref = value;
    return true;
  }

  bool value;
};

class UInt64Value : public Value {
 public:
  explicit UInt64Value(uint64_t value)
      : Value(std::to_string(value)),
        value(value) {
    setTypeId<uint64_t>();
  }

  explicit UInt64Value(const std::string &strvalue)
      : Value(strvalue),
        value(std::stoull(strvalue)) {
    setTypeId<uint64_t>();
  }

  uint64_t getValue() const {
    return value;
  }
 protected:

  virtual bool getValue(int &ref) {
    return false;
  }

  virtual bool getValue(int64_t &ref) {
    if (value <= (std::numeric_limits<int64_t>::max)()) {
      ref = value;
      return true;
    }
    return false;
  }

  virtual bool getValue(uint64_t &ref) {
    ref = value;
    return true;
  }

  virtual bool getValue(bool &ref) {
    return false;
  }

  uint64_t value;
};

class Int64Value : public Value {
 public:
  explicit Int64Value(int64_t value)
      : Value(std::to_string(value)),
        value(value) {
    setTypeId<int64_t>();
  }
  explicit Int64Value(const std::string &strvalue)
      : Value(strvalue),
        value(std::stoll(strvalue)) {
    setTypeId<int64_t>();
  }

  int64_t getValue() {
    return value;
  }
 protected:

  virtual bool getValue(int &ref) {
    return false;
  }

  virtual bool getValue(int64_t &ref) {
    ref = value;
    return true;
  }

  virtual bool getValue(uint64_t &ref) {
    if (value >= 0) {
      ref = value;
      return true;
    }
    return false;
  }

  virtual bool getValue(bool &ref) {
    return false;
  }

  int64_t value;
};

static inline std::shared_ptr<Value> createValue(const bool &object) {
  return std::make_shared<BoolValue>(object);
}

static inline std::shared_ptr<Value> createValue(const char *object) {
  return std::make_shared<Value>(object);
}

static inline std::shared_ptr<Value> createValue(char *object) {
  return std::make_shared<Value>(std::string(object));
}

static inline std::shared_ptr<Value> createValue(const std::string &object) {
  return std::make_shared<Value>(object);
}

static inline std::shared_ptr<Value> createValue(const uint32_t &object) {
  return std::make_shared<UInt64Value>(object);
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

/**
 * Purpose: ValueNode is the AST container for a value
 */
class ValueNode {
 public:
  ValueNode()
      : value_(nullptr) {
  }

  ValueNode(ValueNode &&vn) = default;
  ValueNode(const ValueNode &vn) = default;

  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, int >::value ||
  std::is_same<T, uint32_t >::value ||
  std::is_same<T, size_t >::value ||
  std::is_same<T, uint64_t >::value ||
  std::is_same<T, bool >::value ||
  std::is_same<T, char* >::value ||
  std::is_same<T, const char* >::value ||
  std::is_same<T, std::string>::value,ValueNode&>::type {
    value_ = createValue(ref);
    return *this;
  }

  ValueNode &operator=(const ValueNode &ref) {
    value_ = ref.value_;
    return *this;
  }

  inline bool operator==(const ValueNode &rhs) const {
    return to_string() == rhs.to_string();
  }

  inline bool operator==(const char*rhs) const {
    return to_string() == rhs;
  }

  friend bool operator==(const char *lhs, const ValueNode& rhs) {
    return lhs == rhs.to_string();
  }

  std::string to_string() const {
    return value_ ? value_->getStringValue() : "";
  }

  std::shared_ptr<Value> getValue() const {
    return value_;
  }

  bool empty() const {
    return value_ == nullptr || value_->empty();
  }

 protected:
  std::shared_ptr<Value> value_;
};

struct SerializedResponseNode {
  std::string name;
  ValueNode value;
  bool array;
  bool collapsible;
  std::vector<SerializedResponseNode> children;

  SerializedResponseNode(bool collapsible = true)
      : array(false),
        collapsible(collapsible) {
  }

  SerializedResponseNode(const SerializedResponseNode &other) = default;

  SerializedResponseNode &operator=(const SerializedResponseNode &other) = default;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_VALUE_H_ */
