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
#include <string>
#include <utility>

#include "yaml-cpp/yaml.h"
#include "core/flow/Node.h"
#include "minifi-cpp/utils/gsl.h"


namespace org::apache::nifi::minifi::core {

class YamlNode : public flow::Node::NodeImpl {
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

  flow::Node createEmpty() const override {
    return flow::Node{std::make_shared<YamlNode>(YAML::Node{YAML::NodeType::Undefined})};
  }

  std::optional<std::string> getString() const override {
    if (std::string result; YAML::convert<std::string>::decode(node_, result)) {
      return result;
    }
    if (node_ && node_.IsNull()) {
      return "null";
    }
    return std::nullopt;
  }

  std::optional<bool> getBool() const override {
    if (bool result; YAML::convert<bool>::decode(node_, result)) {
      return result;
    }
    return std::nullopt;
  }

  std::optional<int64_t> getInt64() const override {
    if (int64_t result; YAML::convert<int64_t>::decode(node_, result)) {
      return result;
    }
    return std::nullopt;
  }

  std::optional<std::string> getIntegerAsString() const override {
    return getString();
  }

  std::optional<std::string> getScalarAsString() const override {
    return getString();
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

  std::optional<flow::Node::Cursor> getCursor() const override {
    YAML::Mark mark = node_.Mark();
    if (mark.is_null()) {
      return std::nullopt;
    }
    return flow::Node::Cursor{
      .line = mark.line,
      .column = mark.column,
      .pos = mark.pos
    };
  }

 private:
  YAML::Node node_;
};

class YamlIterator : public flow::Node::Iterator::IteratorImpl {
 public:
  explicit YamlIterator(YAML::const_iterator it) : it_(std::move(it)) {}

  IteratorImpl &operator++() override {
    ++it_;
    return *this;
  }

  bool operator==(const IteratorImpl &other) const override {
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

  std::unique_ptr<IteratorImpl> clone() const override {
    return std::make_unique<YamlIterator>(it_);
  }

 private:
  YAML::const_iterator it_;
};

inline flow::Node::Iterator YamlNode::begin() const {
  return flow::Node::Iterator{std::make_unique<YamlIterator>(node_.begin())};
}

inline flow::Node::Iterator YamlNode::end() const {
  return flow::Node::Iterator{std::make_unique<YamlIterator>(node_.end())};
}

}  // namespace org::apache::nifi::minifi::core
