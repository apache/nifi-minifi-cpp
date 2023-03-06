/**
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

#include <concepts>
#include <utility>

#include "Python.h"

namespace org::apache::nifi::minifi::extensions::python {

template<typename T>
concept convertible_to_object = requires {
  static_cast<PyObject*>(std::declval<T>());
};

template<typename T>
concept custom_type = requires {
  { T::typeObject() } -> std::same_as<PyTypeObject*>;
};

template<typename T>
concept holder_type = requires {
  typename T::HeldType;
} && custom_type<T>;

enum class ReferenceType {
  BORROWED,
  OWNED,
};

template<ReferenceType reference_type>
struct ObjectReference {
  ObjectReference() = default;

  explicit ObjectReference(PyObject* object)
      : object_(object) {
  }

  ~ObjectReference() {
    decrementRefCount();
  }

  ObjectReference(const ObjectReference& that)
      : object_(that.object_) {
    incrementRefCount();
  }

  ObjectReference(ObjectReference&& that)
      : object_(that.object_) {
    that.object_ = nullptr;
  }

  ObjectReference& operator=(const ObjectReference& that) {
    if (this == &that) {
      return *this;
    }

    decrementRefCount();
    object_ = that.object_;
    incrementRefCount();
    return *this;
  }

  ObjectReference& operator=(ObjectReference&& that) {
    if (this == &that) {
      return *this;
    }

    decrementRefCount();
    object_ = that.object_;
    that.object_ = nullptr;
    return *this;
  }

  ObjectReference& operator=(PyObject* object) {
    decrementRefCount();
    object_ = object;
    return *this;
  }

  operator bool() {
    return object_ != nullptr;
  }

  explicit operator PyObject*() {
    return object_;
  }

  const PyObject* get() const {
    return object_;
  }

  PyObject* get() {
    return object_;
  }

  PyObject** asOutParameter() {
    return &object_;
  }

  void reset() requires(reference_type == ReferenceType::OWNED) {
    decrementRefCount();
    object_ = nullptr;
  }

  PyObject* release() {
    auto returnedObject = object_;
    object_ = nullptr;
    return returnedObject;
  }

 private:
  void incrementRefCount() {
    if constexpr (reference_type == ReferenceType::OWNED) {
      Py_XINCREF(object_);
    }
  }

  void decrementRefCount() {
    if constexpr (reference_type == ReferenceType::OWNED) {
      Py_XDECREF(object_);
    }
  }

  PyObject* object_ = nullptr;
};

using OwnedReference = ObjectReference<ReferenceType::OWNED>;
using BorrowedReference = ObjectReference<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class ReferenceHolder {
 public:
  using Reference = ObjectReference<reference_type>;

  ReferenceHolder() = default;

  explicit ReferenceHolder(Reference ref)
      : ref_(std::move(ref)) {
  }

  explicit ReferenceHolder(PyObject* object)
      : ref_(object) {
  }

  operator bool() {
    return static_cast<bool>(ref_);
  }

  explicit operator PyObject*() {
    return ref_.get();
  }

  const PyObject* get() const {
    return ref_.get();
  }

  PyObject* get() {
    return ref_.get();
  }

  PyObject* releaseReference() {
    return ref_.release();
  }

  void resetReference() requires(reference_type == ReferenceType::OWNED) {
    ref_.reset();
  }

 protected:
  Reference ref_;
};

using OwnedReferenceHolder = ReferenceHolder<ReferenceType::OWNED>;
using BorrowedReferenceHolder = ReferenceHolder<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Object;

using OwnedObject = Object<ReferenceType::OWNED>;
using BorrowedObject = Object<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Long;

using OwnedLong = Long<ReferenceType::OWNED>;
using BorrowedLong = Long<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Str;

using OwnedStr = Str<ReferenceType::OWNED>;
using BorrowedStr = Str<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class List;

using OwnedList = List<ReferenceType::OWNED>;
using BorrowedList = List<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Dict;

using OwnedDict = Dict<ReferenceType::OWNED>;
using BorrowedDict = Dict<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Bytes;

using OwnedBytes = Bytes<ReferenceType::OWNED>;
using BorrowedBytes = Bytes<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Capsule;

using OwnedCapsule = Capsule<ReferenceType::OWNED>;
using BorrowedCapsule = Capsule<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Callable;

using OwnedCallable = Callable<ReferenceType::OWNED>;
using BorrowedCallable = Callable<ReferenceType::BORROWED>;

template<ReferenceType reference_type>
class Module;

using OwnedModule = Module<ReferenceType::OWNED>;
using BorrowedModule = Module<ReferenceType::BORROWED>;

}  // namespace org::apache::nifi::minifi::extensions::python
