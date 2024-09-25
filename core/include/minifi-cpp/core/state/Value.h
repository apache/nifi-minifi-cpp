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
#include "minifi-cpp/utils/Export.h"
#include "utils/meta/type_list.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Purpose: Represents an AST value
 * Contains an embedded string representation to be used for a toString analog.
 *
 * Extensions can be more strongly typed and can be used anywhere where an abstract
 * representation is needed.
 */
class Value {
 public:
  virtual ~Value() = default;

  [[nodiscard]] virtual std::string getStringValue() const = 0;

  [[nodiscard]] virtual const char* c_str() const = 0;

  [[nodiscard]] virtual bool empty() const noexcept = 0;

  virtual std::type_index getTypeIndex() = 0;

  virtual bool getValue(uint32_t &ref) = 0;
  virtual bool getValue(int &ref) = 0;
  virtual bool getValue(int64_t &ref) = 0;
  virtual bool getValue(uint64_t &ref) = 0;
  virtual bool getValue(bool &ref) = 0;
  virtual bool getValue(double &ref) = 0;

  MINIFIAPI static const std::type_index UINT64_TYPE;
  MINIFIAPI static const std::type_index INT64_TYPE;
  MINIFIAPI static const std::type_index UINT32_TYPE;
  MINIFIAPI static const std::type_index INT_TYPE;
  MINIFIAPI static const std::type_index BOOL_TYPE;
  MINIFIAPI static const std::type_index DOUBLE_TYPE;
  MINIFIAPI static const std::type_index STRING_TYPE;
};

/**
 * Purpose: ValueNode is the AST container for a value
 */
class ValueNode {
  using supported_types = utils::meta::type_list<int, uint32_t, size_t, int64_t, uint64_t, bool, char*, const char*, double, std::string>;

 public:
  ValueNode() = default;

  template<typename T>
  requires (supported_types::contains<T>())  // NOLINT
  /* implicit, because it doesn't change the meaning, and it simplifies construction of maps */
  ValueNode(const T value);  // NOLINT

  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  requires (supported_types::contains<T>())  // NOLINT
  ValueNode& operator=(const T ref);

  inline bool operator==(const ValueNode &rhs) const {
    return to_string() == rhs.to_string();
  }

  inline bool operator==(const char* rhs) const {
    return to_string() == rhs;
  }

  friend bool operator==(const char* lhs, const ValueNode& rhs) {
    return lhs == rhs.to_string();
  }

  [[nodiscard]] std::string to_string() const {
    return value_ ? value_->getStringValue() : "";
  }

  [[nodiscard]] std::shared_ptr<Value> getValue() const {
    return value_;
  }

  [[nodiscard]] bool empty() const noexcept {
    return value_ == nullptr || value_->empty();
  }

 protected:
  std::shared_ptr<Value> value_;
};

struct SerializedResponseNode {
  std::string name;
  ValueNode value{};
  bool array = false;
  bool collapsible = true;
  bool keep_empty = false;
  std::vector<SerializedResponseNode> children{};

  [[nodiscard]] bool empty() const noexcept {
    return value.empty() && children.empty();
  }

  [[nodiscard]] std::string to_string() const;

  [[nodiscard]] std::string to_pretty_string() const;
};

}  // namespace org::apache::nifi::minifi::state::response
