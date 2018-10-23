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
#ifndef LIBMINIFI_INCLUDE_C2_PAYLOADPARSER_H_
#define LIBMINIFI_INCLUDE_C2_PAYLOADPARSER_H_

#include "C2Payload.h"
#include "core/state/Value.h"
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

class PayloadParseException : public std::runtime_error {
 public:
  PayloadParseException(const std::string &msg)
      : std::runtime_error(msg) {
  }

};

template<typename T, typename C>
class convert_if_base {
 protected:
  const std::shared_ptr<state::response::Value> node_;
  explicit convert_if_base(const std::shared_ptr<state::response::Value> &node)
      : node_(node) {
  }
 public:
  T operator()() const {
    if (auto sub_type = std::dynamic_pointer_cast<C>(node_))
      return sub_type->getValue();
    throw PayloadParseException("No known type");
  }
};

template<typename T>
struct convert_if {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node) {
  }


  std::string operator()() const {
    throw PayloadParseException("No known type");
  }
};

template<>
struct convert_if<std::string> : public convert_if_base<std::string, state::response::Value> {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node)
      : convert_if_base(node) {
  }

  std::string operator()() const {
    return node_->getStringValue();
  }
};

template<>
struct convert_if<uint64_t> : public convert_if_base<uint64_t, state::response::UInt64Value> {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node)
      : convert_if_base(node) {
  }
};

template<>
struct convert_if<int64_t> : public convert_if_base<int64_t, state::response::Int64Value> {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node)
      : convert_if_base(node) {
  }
};

template<>
struct convert_if<int> : public convert_if_base<int, state::response::IntValue> {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node)
      : convert_if_base(node) {
  }
};

template<>
struct convert_if<bool> : public convert_if_base<bool, state::response::BoolValue> {
  explicit convert_if(const std::shared_ptr<state::response::Value> &node)
      : convert_if_base(node) {
  }
};

/**
 * Defines a fluent parser that uses Exception management for flow control.
 *
 * Note that this isn't functionally complete.
 */
class PayloadParser {

 public:

  static PayloadParser getInstance(const C2Payload &payload) {
    return PayloadParser(payload);
  }

  inline PayloadParser in(const std::string &payload) {
    for (const auto &pl : ref_.getNestedPayloads()) {
      if (pl.getLabel() == payload || pl.getIdentifier() == payload) {
        return PayloadParser(pl);
      }
    }
    throw PayloadParseException("Invalid payload. Could not find " + payload);
  }

  template<typename Functor>
  inline void foreach(Functor f) {
    for (const auto &component : ref_.getNestedPayloads()) {
      f(component);
    }
  }

  template<typename T>
  inline T getAs(const std::string &field) {
    for (const auto &cmd : ref_.getContent()) {
      auto exists = cmd.operation_arguments.find(field);
      if (exists != cmd.operation_arguments.end()) {
        return convert_if<T>(exists->second.getValue())();
      }
    }
    std::stringstream ss;
    ss << "Invalid Field. Could not find " << field << " in " << ref_.getLabel();
    throw PayloadParseException(ss.str());
  }

  template<typename T>
  inline T getAs(const std::string &field, const T &fallback) {
    for (const auto &cmd : ref_.getContent()) {
      auto exists = cmd.operation_arguments.find(field);
      if (exists != cmd.operation_arguments.end()) {
        return convert_if<T>(exists->second.getValue())();
      }
    }
    return fallback;
  }

  size_t getSize() const {
    return ref_.getNestedPayloads().size();
  }

  /**
   * Make these explicitly public.
   */

  PayloadParser(const PayloadParser &p) = delete;
  const PayloadParser &operator=(const PayloadParser &p) = delete;
  PayloadParser(PayloadParser &&parser) = default;

 private:

  PayloadParser(const C2Payload &payload)
      : ref_(payload) {
  }

  const C2Payload &ref_;

  std::vector<std::string> fields_;

  std::string component_to_get_;
};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_PAYLOADPARSER_H_ */
