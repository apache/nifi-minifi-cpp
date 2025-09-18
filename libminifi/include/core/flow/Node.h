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

#include <string_view>
#include <tuple>
#include <optional>
#include <string>
#include <memory>
#include <utility>
#include "nonstd/expected.hpp"
#include "utils/StringUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core::flow {

class Node {
 public:
  struct Cursor {
    int line{0};
    int column{0};
    int pos{0};
  };

  class Iterator {
    friend class Node;
   public:
    class Value;

    class IteratorImpl {
     public:
      virtual IteratorImpl& operator++() = 0;
      virtual bool operator==(const IteratorImpl& other) const = 0;
      virtual Value operator*() const = 0;
      bool operator!=(const IteratorImpl& other) const {return !(*this == other);}

      virtual std::unique_ptr<IteratorImpl> clone() const = 0;
      virtual ~IteratorImpl() = default;
      std::string path_;
    };

    Iterator& operator++() {
      impl_->operator++();
      ++idx_;
      return *this;
    }

    explicit Iterator(std::unique_ptr<IteratorImpl> impl) : impl_(std::move(impl)) {}
    Iterator(const Iterator& other): impl_(other.impl_->clone()) {}
    Iterator(Iterator&&) = default;
    Iterator& operator=(const Iterator& other) {
      if (this == &other) {
        return *this;
      }
      impl_ = other.impl_->clone();
      return *this;
    }
    Iterator& operator=(Iterator&&) = default;

    bool operator==(const Iterator& other) const {return impl_->operator==(*other.impl_);}
    bool operator!=(const Iterator& other) const {return !(*this == other);}

    Value operator*() const;

   private:
    std::unique_ptr<IteratorImpl> impl_;
    int idx_{0};
  };

  class NodeImpl {
   public:
    virtual explicit operator bool() const = 0;
    virtual bool isSequence() const = 0;
    virtual bool isMap() const = 0;
    virtual bool isNull() const = 0;

    [[nodiscard]] virtual std::optional<std::string> getString() const = 0;
    [[nodiscard]] virtual std::optional<bool> getBool() const = 0;
    [[nodiscard]] virtual std::optional<int64_t> getInt64() const = 0;

    [[nodiscard]] virtual std::optional<std::string> getIntegerAsString() const = 0;
    [[nodiscard]] virtual std::optional<std::string> getScalarAsString() const = 0;

    virtual std::string getDebugString() const = 0;

    virtual size_t size() const = 0;
    virtual Iterator begin() const = 0;
    virtual Iterator end() const = 0;
    virtual Node operator[](std::string_view key) const = 0;

    virtual std::optional<Cursor> getCursor() const = 0;

    virtual Node createEmpty() const = 0;

    virtual ~NodeImpl() = default;

    std::string path_;
  };

  Node() = default;
  explicit Node(std::shared_ptr<NodeImpl> impl): impl_(std::move(impl)) {}

  explicit operator bool() const {return impl_->operator bool();}
  bool isSequence() const {return impl_->isSequence();}
  bool isMap() const {return impl_->isMap();}
  bool isNull() const {return impl_->isNull();}

  [[nodiscard]] std::optional<std::string> getString() const {return impl_->getString(); }
  [[nodiscard]] std::optional<bool> getBool() const { return impl_->getBool(); }
  [[nodiscard]] std::optional<int64_t> getInt64() const { return impl_->getInt64(); }

  [[nodiscard]] std::optional<std::string> getIntegerAsString() const { return impl_->getIntegerAsString(); }
  [[nodiscard]] std::optional<std::string> getScalarAsString() const { return impl_->getScalarAsString(); }

  // return a string representation of the node (need not be deserializable)
  std::string getDebugString() const {return impl_->getDebugString();}

  size_t size() const {return impl_->size();}
  size_t empty() const {
    return size() == 0;
  }
  Iterator begin() const {
    Iterator it = impl_->begin();
    it.impl_->path_ = impl_->path_;
    return it;
  }
  Iterator end() const {
    Iterator it = impl_->end();
    it.impl_->path_ = impl_->path_;
    return it;
  }

  // considers @key to be a member of this node as is
  Node getMember(std::string_view key) {
    Node result = impl_->operator[](key);
    result.impl_->path_ = utils::string::join_pack(impl_->path_, "/", key);
    return result;
  }

  // considers @key to be a '/'-delimited access path
  Node operator[](std::string_view key) const {
    Node result = *this;
    for (auto& field : utils::string::split(std::string{key}, "/")) {
      if (key == ".") {
        // pass: self
      } else {
        result = result.getMember(field);
      }
      if (!result) {
        break;
      }
    }
    return result;
  }

  // considers @keys to be a set of viable access paths, the first viable is returned
  Node operator[](std::span<const std::string> keys) const {
    for (auto& key : keys) {
      if (Node result = (*this)[key]) {
        return result;
      }
    }
    return impl_->createEmpty();
  }

  std::string getPath() const {return impl_->path_;}

  std::optional<Cursor> getCursor() const {return impl_->getCursor();}

 private:
  std::shared_ptr<NodeImpl> impl_;
};

class Node::Iterator::Value : public Node, public std::pair<Node, Node> {
 public:
  Value(Node node, Node key, Node value): Node{std::move(node)}, std::pair<Node, Node>{std::move(key), std::move(value)} {}
};

}  // namespace org::apache::nifi::minifi::core::flow
