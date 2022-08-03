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
#include "nonstd/expected.hpp"

namespace org::apache::nifi::minifi::core::flow {

class Node {
 public:
  class Impl;
  class Iterator {
   public:
    class Value;

    class Impl {
     public:
      virtual Impl& operator++() = 0;
      virtual bool operator==(const Impl& other) const = 0;
      virtual Value operator*() const = 0;
      bool operator!=(const Impl& other) const {return !(*this == other);}

      virtual std::unique_ptr<Impl> clone() const = 0;
      virtual ~Impl() = default;
    };

    Iterator& operator++() {
      impl_->operator++();
      return *this;
    }

    explicit Iterator(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
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
    std::unique_ptr<Impl> impl_;
  };

  class Impl {
   public:
    virtual explicit operator bool() const = 0;
    virtual bool isSequence() const = 0;
    virtual bool isMap() const = 0;
    virtual bool isNull() const = 0;
    virtual bool isScalar() const = 0;

    virtual nonstd::expected<std::string, std::exception_ptr> getString() const = 0;
    virtual nonstd::expected<int, std::exception_ptr> getInt() const = 0;
    virtual nonstd::expected<unsigned int, std::exception_ptr> getUInt() const = 0;
    virtual nonstd::expected<bool, std::exception_ptr> getBool() const = 0;
    virtual nonstd::expected<int64_t, std::exception_ptr> getInt64() const = 0;
    virtual nonstd::expected<uint64_t, std::exception_ptr> getUInt64() const = 0;

    virtual std::string getDebugString() const = 0;

    virtual size_t size() const = 0;
    virtual Iterator begin() const = 0;
    virtual Iterator end() const = 0;
    virtual Node operator[](std::string_view key) const = 0;
    virtual ~Impl() = default;
  };

  Node() = default;
  explicit Node(std::shared_ptr<Impl> impl): impl_(std::move(impl)) {}

  explicit operator bool() const {return impl_->operator bool();}
  bool isSequence() const {return impl_->isSequence();}
  bool isMap() const {return impl_->isMap();}
  bool isNull() const {return impl_->isNull();}
  bool isScalar() const {return impl_->isScalar();}

  nonstd::expected<std::string, std::exception_ptr> getString() const {return impl_->getString();}
  nonstd::expected<bool, std::exception_ptr> getBool() const {return impl_->getBool();}
  nonstd::expected<int, std::exception_ptr> getInt() const {return impl_->getInt();}
  nonstd::expected<unsigned int, std::exception_ptr> getUInt() const {return impl_->getUInt();}
  nonstd::expected<int64_t, std::exception_ptr> getInt64() const {return impl_->getInt64();}
  nonstd::expected<uint64_t, std::exception_ptr> getUInt64() const {return impl_->getUInt64();}

  // return a string representation of the node (need not to be deserializable)
  std::string getDebugString() const {return impl_->getDebugString();}

  size_t size() const {return impl_->size();}
  size_t empty() const {
    return size() == 0;
  }
  Iterator begin() const {return impl_->begin();}
  Iterator end() const {return impl_->end();}
  Node operator[](std::string_view key) const {return impl_->operator[](key);}

 private:
  std::shared_ptr<Impl> impl_;
};

class Node::Iterator::Value : public Node, public std::pair<Node, Node> {
 public:
  Value(Node node, Node key, Node value): Node{std::move(node)}, std::pair<Node, Node>{std::move(key), std::move(value)} {}
};

}  // namespace org::apache::nifi::minifi::core::flow
