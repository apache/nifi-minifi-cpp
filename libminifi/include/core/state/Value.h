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

#include <memory>
#include <string>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Purpose: Represents an AST value
 * Contains an embedded string representation to be used for a toString analog.
 */
class Value {
 public:

  explicit Value(const std::string &value)
      : string_value(value) {

  }

  virtual ~Value() {

  }
  std::string getStringValue() const {
    return string_value;
  }

 protected:
  std::string string_value;

};

class IntValue : public Value {
 public:
  explicit IntValue(int value)
      : Value(std::to_string(value)),
        value(value) {

  }
  int getValue() {
    return value;
  }
 protected:
  int value;
};

class BoolValue : public Value {
 public:
  explicit BoolValue(bool value)
      : Value(value ? "true" : "false"),
        value(value) {

  }
  bool getValue() {
    return value;
  }
 protected:
  bool value;
};

class Int64Value : public Value {
 public:
  explicit Int64Value(uint64_t value)
      : Value(std::to_string(value)),
        value(value) {

  }
  uint64_t getValue() {
    return value;
  }
 protected:
  uint64_t value;
};


static inline std::shared_ptr<Value> createValue(
    const bool &object) {
  return std::make_shared<BoolValue>(object);
}

static inline std::shared_ptr<Value> createValue(
    const char *object) {
  return std::make_shared<Value>(object);
}

static inline std::shared_ptr<Value> createValue(
    char *object) {
  return std::make_shared<Value>(std::string(object));
}

static inline std::shared_ptr<Value> createValue(
    const std::string &object) {
  return std::make_shared<Value>(object);
}


static inline std::shared_ptr<Value> createValue(
    const uint32_t &object) {
  return std::make_shared<Int64Value>(object);
}
static inline std::shared_ptr<Value> createValue(
    const uint64_t &object) {
  return std::make_shared<Int64Value>(object);
}

static inline std::shared_ptr<Value> createValue(
    const int &object) {
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

  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  auto operator=(
      const T ref) -> typename std::enable_if<std::is_same<T, int >::value ||
      std::is_same<T, uint32_t >::value ||
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
    return value_ == nullptr;
  }

 protected:
  std::shared_ptr<Value> value_;
};

struct SerializedResponseNode {
  std::string name;
  ValueNode value;

  std::vector<SerializedResponseNode> children;
  SerializedResponseNode &operator=(const SerializedResponseNode &other) {
    name = other.name;
    value = other.value;
    children = other.children;
    return *this;
  }
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_VALUE_H_ */
