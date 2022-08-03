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

#include "yaml-cpp/yaml.h"
#include "core/flow/Node.h"
#include "utils/gsl.h"


namespace org::apache::nifi::minifi::core {

class YamlNode : public flow::Node::Impl {
 public:
  explicit YamlNode(YAML::Node node) : node_(std::move(node)) {}

  explicit operator bool() const override {
    return node_.operator bool();
  }

  bool isSequence() const override {
    return node_.IsSequence();
  }

  bool isMap() const override {
    return node_.IsMap();
  }

  bool isNull() const override {
    return node_.IsNull();
  }

  bool isScalar() const override {
    return node_.IsScalar();
  }

  nonstd::expected<std::string, std::exception_ptr> getString() const override {
    try {
      return node_.as<std::string>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  nonstd::expected<int, std::exception_ptr> getInt() const override {
    try {
      return node_.as<int>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  nonstd::expected<unsigned int, std::exception_ptr> getUInt() const override {
    try {
      return node_.as<unsigned int>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  nonstd::expected<bool, std::exception_ptr> getBool() const override {
    try {
      return node_.as<bool>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  nonstd::expected<int64_t, std::exception_ptr> getInt64() const override {
    try {
      return node_.as<int64_t>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  nonstd::expected<uint64_t, std::exception_ptr> getUInt64() const override {
    try {
      return node_.as<uint64_t>();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  std::string getDebugString() const override {
    if (!node_) return "<invalid>";
    if (node_.IsNull()) return "null";
    if (node_.IsSequence()) return "<Array>";
    if (node_.IsMap()) return "<Map>";
    if (node_.IsScalar()) return '"' + node_.Scalar() + '"';
    return "<unknown>";
  }

  size_t size() const override {
    return node_.size();
  }

  flow::Node::Iterator begin() const override;

  flow::Node::Iterator end() const override;

  flow::Node operator[](std::string_view key) const override {
    return flow::Node{std::make_shared<YamlNode>(node_[std::string{key}])};
  }

 private:
  YAML::Node node_;
};

class YamlIterator : public flow::Node::Iterator::Impl {
 public:
  explicit YamlIterator(YAML::const_iterator it) : it_(std::move(it)) {}

  Impl &operator++() override {
    ++it_;
    return *this;
  }

  bool operator==(const Impl &other) const override {
    const auto *ptr = dynamic_cast<const YamlIterator *>(&other);
    gsl_Expects(ptr);
    return it_ == ptr->it_;
  }

  flow::Node::Iterator::Value operator*() const override {
    auto val = *it_;
    auto node = flow::Node{std::make_shared<YamlNode>(val)};
    auto first = flow::Node{std::make_shared<YamlNode>(val.first)};
    auto second = flow::Node{std::make_shared<YamlNode>(val.second)};
    return flow::Node::Iterator::Value(node, first, second);
  }

  std::unique_ptr<Impl> clone() const override {
    return std::make_unique<YamlIterator>(it_);
  }

 private:
  YAML::const_iterator it_;
};

flow::Node::Iterator YamlNode::begin() const {
  return flow::Node::Iterator{std::make_unique<YamlIterator>(node_.begin())};
}

flow::Node::Iterator YamlNode::end() const {
  return flow::Node::Iterator{std::make_unique<YamlIterator>(node_.end())};
}

}  // namespace org::apache::nifi::minifi::core
