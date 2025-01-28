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

#include <string>
#include <utility>
#include <memory>

#include "core/flow/Node.h"
#include "rapidjson/document.h"
#include "utils/gsl.h"
#include "utils/ValueCaster.h"

namespace org::apache::nifi::minifi::core {

class JsonNode : public flow::Node::NodeImpl {
 public:
  explicit JsonNode(rapidjson::Value* node, rapidjson::Document::AllocatorType& alloc): node_(node), allocator_(alloc) {}

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

  flow::Node createEmpty() const override {
    return flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
  }

  [[nodiscard]] std::optional<std::string> getString() const override {
    if (node_ && node_->IsString()) {
      return std::string{node_->GetString(), node_->GetStringLength()};
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<int64_t> getInt64() const override {
    if (node_ && node_->IsInt64()) {
      return node_->GetInt64();
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<bool> getBool() const override {
    if (node_ && node_->IsBool()) {
      return node_->GetBool();
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<std::string> getIntegerAsString() const override {
    if (!node_) {
      return std::nullopt;
    }
    if (node_->IsInt64()) {
      return std::to_string(node_->GetInt64());
    }
    if (node_->IsUint64()) {
      return std::to_string(node_->GetUint64());
    }
    return getString();
  }

  [[nodiscard]] std::optional<std::string> getScalarAsString() const override {
    if (!node_) {
      return std::nullopt;
    }
    if (node_->IsInt64()) {
      return std::to_string(node_->GetInt64());
    }
    if (node_->IsUint64()) {
      return std::to_string(node_->GetUint64());
    }
    if (node_->IsDouble()) {
      return std::to_string(node_->GetDouble());
    }
    if (node_->IsBool()) {
      return node_->GetBool() ? "true" : "false";
    }

    return getString();
  }

  std::string getDebugString() const override {
    if (!node_) return "<invalid>";
    if (node_->IsObject()) return "<Map>";
    if (node_->IsArray()) return "<Array>";
    if (node_->IsNull()) return "null";
    if (auto int_str = getIntegerAsString()) {
      return int_str.value();
    }
    if (node_->IsTrue()) return "true";
    if (node_->IsFalse()) return "false";
    if (node_->IsDouble()) return std::to_string(node_->GetDouble());
    if (node_->IsString()) return '"' + std::string(node_->GetString(), node_->GetStringLength()) + '"';
    return "<unknown>";
  }

  size_t size() const override {
    if (!node_) {
      throw std::runtime_error(fmt::format("Cannot get size of invalid json value at '{}'", path_));
    }
    if (!node_->IsArray()) {
      throw std::runtime_error(fmt::format("Cannot get size of non-array json value at '{}'", path_));
    }
    return node_->Size();
  }
  flow::Node::Iterator begin() const override;
  flow::Node::Iterator end() const override;

  flow::Node operator[](std::string_view key) const override {
    if (!node_ || node_->IsArray() || node_->IsNull()) {
      return flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
    }
    if (!node_->IsObject()) {
      throw std::runtime_error(fmt::format("Cannot get member '{}' of scalar json value at '{}'", key, path_));
    }
    auto it = node_->FindMember(rapidjson::Value(rapidjson::StringRef(key.data(), key.length())));
    if (it == node_->MemberEnd()) {
      return flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
    }
    return flow::Node{std::make_shared<JsonNode>(&it->value, allocator_)};
  }

  [[nodiscard]] bool contains(const std::string_view key) const override {
    return node_ && node_->HasMember(key.data());
  }

  [[nodiscard]] bool remove(const std::string_view key) override {
    return node_->RemoveMember(key.data());
  }

  std::optional<flow::Node> pushBack() override {
    if (!node_->IsArray()) {
      return std::nullopt;
    }
    rapidjson::Value value(rapidjson::kObjectType);
    auto& new_value = node_->PushBack(rapidjson::Value{rapidjson::kObjectType}, allocator_)[node_->Size() - 1];
    return flow::Node{std::make_shared<JsonNode>(&new_value, allocator_)};
  }

  std::optional<flow::Node> addMember(const std::string_view key, const std::string_view value) override {
    if (!node_ || !node_->IsObject()) {
      throw std::runtime_error("Not an object");
    }
    auto& new_node = node_->AddMember(rapidjson::Value(key.data(), allocator_), rapidjson::Value(value.data(), allocator_), allocator_)[key.data()];
    return flow::Node{std::make_shared<JsonNode>(&new_node, allocator_)};
  }

  std::optional<flow::Node> addObject(const std::string_view key) override {
    if (!node_ || !node_->IsObject()) {
      throw std::runtime_error("Not an object");
    }
    auto& new_node = node_->AddMember(rapidjson::Value(key.data(), allocator_), rapidjson::Value(rapidjson::kObjectType), allocator_)[key.data()];
    return flow::Node{std::make_shared<JsonNode>(&new_node, allocator_)};
  }

  std::optional<flow::Node::Cursor> getCursor() const override {
    return std::nullopt;
  }

 private:
  rapidjson::Value* node_;
  rapidjson::Document::AllocatorType& allocator_;
};

class JsonValueIterator : public flow::Node::Iterator::IteratorImpl {
 public:
  explicit JsonValueIterator(rapidjson::Value::ValueIterator it, rapidjson::Document::AllocatorType& alloc): it_(std::move(it)), allocator_(alloc) {}

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
    auto node = flow::Node{std::make_shared<JsonNode>(&*it_, allocator_)};
    auto first = flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
    auto second = flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
    return {std::move(node), std::move(first), std::move(second)};
  }

  std::unique_ptr<IteratorImpl> clone() const override {
    return std::make_unique<JsonValueIterator>(it_, allocator_);
  }

 private:
  rapidjson::Value::ValueIterator it_;
  rapidjson::Document::AllocatorType& allocator_;
};

class JsonMemberIterator : public flow::Node::Iterator::IteratorImpl {
 public:
  explicit JsonMemberIterator(rapidjson::Value::MemberIterator it, rapidjson::Document::AllocatorType& alloc): it_(std::move(it)), allocator_(alloc) {}

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
    auto node = flow::Node{std::make_shared<JsonNode>(nullptr, allocator_)};
    auto first = flow::Node{std::make_shared<JsonNode>(&it_->name, allocator_)};
    auto second = flow::Node{std::make_shared<JsonNode>(&it_->value, allocator_)};
    return flow::Node::Iterator::Value(node, first, second);
  }

  std::unique_ptr<IteratorImpl> clone() const override {
    return std::make_unique<JsonMemberIterator>(it_, allocator_);
  }

 private:
  rapidjson::Value::MemberIterator it_;
  rapidjson::Document::AllocatorType& allocator_;
};

inline flow::Node::Iterator JsonNode::begin() const {
  if (!node_) {
    throw std::runtime_error(fmt::format("Cannot get begin of invalid json value at '{}'", path_));
  }
  if (node_->IsArray()) {
    return flow::Node::Iterator{std::make_unique<JsonValueIterator>(node_->Begin(), allocator_)};
  } else if (node_->IsObject()) {
    return flow::Node::Iterator{std::make_unique<JsonMemberIterator>(node_->MemberBegin(), allocator_)};
  } else {
    throw std::runtime_error(fmt::format("Json node is not iterable, neither array nor object at '{}'", path_));
  }
}

inline flow::Node::Iterator JsonNode::end() const {
  if (!node_) {
    throw std::runtime_error(fmt::format("Cannot get end of invalid json value at '{}'", path_));
  }
  if (node_->IsArray()) {
    return flow::Node::Iterator{std::make_unique<JsonValueIterator>(node_->End(), allocator_)};
  } else if (node_->IsObject()) {
    return flow::Node::Iterator{std::make_unique<JsonMemberIterator>(node_->MemberEnd(), allocator_)};
  } else {
    throw std::runtime_error(fmt::format("Json node is not iterable, neither array nor object at '{}'", path_));
  }
}

}  // namespace org::apache::nifi::minifi::core
