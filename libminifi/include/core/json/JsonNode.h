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

#include "core/flow/Node.h"
#include "rapidjson/document.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

class JsonNode : public flow::Node::NodeImpl {
 public:
  explicit JsonNode(const rapidjson::Value* node): node_(node) {}

  explicit operator bool() const override {
    return node_ != nullptr;
  }
  bool isSequence() const override {
    return node_ ? node_->IsArray() : false;
  }
  bool isMap() const override {
    return node_ ? node_->IsObject() : false;
  }
  bool isNull() const override {
    return node_ ? node_->IsNull() : false;
  }

  nonstd::expected<std::string, std::exception_ptr> getString() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get string of invalid json value");
      }
      if (!node_->IsString()) {
        throw std::runtime_error("Cannot get string of non-string json value");
      }
      return std::string{node_->GetString(), node_->GetStringLength()};
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }
  nonstd::expected<int, std::exception_ptr> getInt() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get int of invalid json value");
      }
      if (!node_->IsInt()) {
        throw std::runtime_error("Cannot get int of non-int json value");
      }
      return node_->GetInt();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }
  nonstd::expected<unsigned int, std::exception_ptr> getUInt() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get unsigned int of invalid json value");
      }
      if (!node_->IsUint()) {
        throw std::runtime_error("Cannot get unsigned int of non-unsigned int json value");
      }
      return node_->GetUint();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }
  nonstd::expected<bool, std::exception_ptr> getBool() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get bool of invalid json value");
      }
      if (!node_->IsBool()) {
        throw std::runtime_error("Cannot get bool of non-bool json value");
      }
      return node_->GetBool();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }
  nonstd::expected<int64_t, std::exception_ptr> getInt64() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get int64 of invalid json value");
      }
      if (!node_->IsInt64()) {
        throw std::runtime_error("Cannot get int64 of non-int64 json value");
      }
      return node_->GetInt64();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }
  nonstd::expected<uint64_t, std::exception_ptr> getUInt64() const override {
    try {
      if (!node_) {
        throw std::runtime_error("Cannot get uint64 of invalid json value");
      }
      if (!node_->IsUint64()) {
        throw std::runtime_error("Cannot get uint64 of non-uint64 json value");
      }
      return node_->GetUint64();
    } catch (...) {
      return nonstd::make_unexpected(std::current_exception());
    }
  }

  std::string getDebugString() const override {
    if (!node_) return "<invalid>";
    if (node_->IsObject()) return "<Map>";
    if (node_->IsArray()) return "<Array>";
    if (node_->IsNull()) return "null";
    if (node_->IsInt()) return std::to_string(node_->GetInt());
    if (node_->IsUint()) return std::to_string(node_->GetUint());
    if (node_->IsInt64()) return std::to_string(node_->GetInt64());
    if (node_->IsUint64()) return std::to_string(node_->GetUint64());
    if (node_->IsTrue()) return "true";
    if (node_->IsFalse()) return "false";
    if (node_->IsDouble()) return std::to_string(node_->GetDouble());
    if (node_->IsString()) return '"' + std::string(node_->GetString(), node_->GetStringLength()) + '"';
    return "<unknown>";
  }

  size_t size() const override {
    if (!node_) {
      throw std::runtime_error("Cannot get size of invalid json value");
    }
    if (!node_->IsArray()) {
      throw std::runtime_error("Cannot get size of non-array json value");
    }
    return node_->Size();
  }
  flow::Node::Iterator begin() const override;
  flow::Node::Iterator end() const override;

  flow::Node operator[](std::string_view key) const override {
    if (!node_) {
      throw std::runtime_error("Cannot get member of invalid json value");
    }
    if (!node_->IsObject()) {
      return flow::Node{std::make_shared<JsonNode>(nullptr)};
    }
    auto it = node_->FindMember(rapidjson::Value(rapidjson::StringRef(key.data(), key.length())));
    if (it == node_->MemberEnd()) {
      return flow::Node{std::make_shared<JsonNode>(nullptr)};
    }
    return flow::Node{std::make_shared<JsonNode>(&it->value)};
  }

  std::optional<flow::Node::Cursor> getCursor() const override {
    return std::nullopt;
  }

 private:
  const rapidjson::Value* node_;
};

class JsonValueIterator : public flow::Node::Iterator::IteratorImpl {
 public:
  explicit JsonValueIterator(rapidjson::Value::ConstValueIterator it): it_(std::move(it)) {}

  IteratorImpl& operator++() override {
    ++it_;
    return *this;
  }
  bool operator==(const IteratorImpl& other) const override {
    const auto* ptr = dynamic_cast<const JsonValueIterator*>(&other);
    gsl_Expects(ptr);
    return it_ == ptr->it_;
  }
  flow::Node::Iterator::Value operator*() const override {
    auto node = flow::Node{std::make_shared<JsonNode>(&*it_)};
    auto first = flow::Node{std::make_shared<JsonNode>(nullptr)};
    auto second = flow::Node{std::make_shared<JsonNode>(nullptr)};
    return {std::move(node), std::move(first), std::move(second)};
  }

  std::unique_ptr<IteratorImpl> clone() const override {
    return std::make_unique<JsonValueIterator>(it_);
  }

 private:
  rapidjson::Value::ConstValueIterator it_;
};

class JsonMemberIterator : public flow::Node::Iterator::IteratorImpl {
 public:
  explicit JsonMemberIterator(rapidjson::Value::ConstMemberIterator it): it_(std::move(it)) {}

  IteratorImpl& operator++() override {
    ++it_;
    return *this;
  }
  bool operator==(const IteratorImpl& other) const override {
    const auto* ptr = dynamic_cast<const JsonMemberIterator*>(&other);
    gsl_Expects(ptr);
    return it_ == ptr->it_;
  }
  flow::Node::Iterator::Value operator*() const override {
    auto node = flow::Node{std::make_shared<JsonNode>(nullptr)};
    auto first = flow::Node{std::make_shared<JsonNode>(&it_->name)};
    auto second = flow::Node{std::make_shared<JsonNode>(&it_->value)};
    return flow::Node::Iterator::Value(node, first, second);
  }

  std::unique_ptr<IteratorImpl> clone() const override {
    return std::make_unique<JsonMemberIterator>(it_);
  }

 private:
  rapidjson::Value::ConstMemberIterator it_;
};

inline flow::Node::Iterator JsonNode::begin() const {
  if (!node_) {
    throw std::runtime_error("Cannot get begin of invalid json value");
  }
  if (node_->IsArray()) {
    return flow::Node::Iterator{std::make_unique<JsonValueIterator>(node_->Begin())};
  } else if (node_->IsObject()) {
    return flow::Node::Iterator{std::make_unique<JsonMemberIterator>(node_->MemberBegin())};
  } else {
    throw std::runtime_error("Json node is not iterable, neither array nor object");
  }
}

inline flow::Node::Iterator JsonNode::end() const {
  if (!node_) {
    throw std::runtime_error("Cannot get end of invalid json value");
  }
  if (node_->IsArray()) {
    return flow::Node::Iterator{std::make_unique<JsonValueIterator>(node_->End())};
  } else if (node_->IsObject()) {
    return flow::Node::Iterator{std::make_unique<JsonMemberIterator>(node_->MemberEnd())};
  } else {
    throw std::runtime_error("Json node is not iterable, neither array nor object");
  }
}

}  // namespace org::apache::nifi::minifi::core
