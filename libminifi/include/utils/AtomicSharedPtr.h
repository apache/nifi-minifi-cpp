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

#include <memory>
#include <mutex>
#include <atomic>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/*
 * Provides a subset of the interface of the std::atomic<std::shared_ptr<T>>
 * specialization (C++20), so we should be able to eliminate this class and
 * use that from C++20.
 */
template<typename T>
class AtomicSharedPtr {
 public:
  using value_type = std::shared_ptr<T>;

  static constexpr bool is_always_lock_free = false;

  constexpr AtomicSharedPtr() noexcept = default;
  AtomicSharedPtr(std::shared_ptr<T> desired) noexcept : value_{desired} {}
  AtomicSharedPtr(const AtomicSharedPtr&) = delete;
  void operator=(const AtomicSharedPtr&) = delete;

  void store(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    std::lock_guard<std::mutex> guard(mtx_);
    value_.swap(desired);
  }

  std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    std::lock_guard<std::mutex> guard(mtx_);
    return value_;
  }

  std::shared_ptr<T> exchange(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    std::lock_guard<std::mutex> guard(mtx_);
    value_.swap(desired);
    return desired;
  }

  void operator=(std::shared_ptr<T> desired) noexcept {  // NOLINT
    store(desired);
  }

  operator std::shared_ptr<T>() const noexcept {  // NOLINT
    return load();
  }

  bool is_lock_free() const noexcept {
    return false;
  }

 private:
  mutable std::mutex mtx_;
  std::shared_ptr<T> value_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
