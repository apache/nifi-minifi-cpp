// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdlib>
#ifdef WIN32
#include <WS2tcpip.h>
#else
#include <ifaddrs.h>
#endif /* WIN32 */
#include <memory>
#include <utility>

namespace org::apache::nifi::minifi::utils {

struct FreeDeleter {
  void operator()(void* const ptr) const noexcept {
    // free(null) is guaranteed to be safe, so no need to be defensive.
    free(ptr);
  }
};

template<typename T>
using freeing_unique_ptr = std::unique_ptr<T, FreeDeleter>;

/**
 * Allows smart pointers to store a pointer both
 * to the stack and the heap while ensuring selective
 * destruction of only heap allocated objects.
 * @tparam T type of the object
 * @tparam D type of the deleter used for heap instances
 */
template<typename T, typename D>
struct StackAwareDeleter {
  template<typename ...Args>
  explicit StackAwareDeleter(T* stack_instance, Args&& ...args)
    : stack_instance_{stack_instance},
      impl_{std::forward<Args>(args)...} {}

  void operator()(T* const ptr) const noexcept(noexcept(impl_(ptr))) {
    if (ptr != stack_instance_) {
      impl_(ptr);
    }
  }

 private:
  T* stack_instance_;
  D impl_;
};

#ifndef WIN32
struct ifaddrs_deleter {
  void operator()(ifaddrs* const p) const noexcept {
    freeifaddrs(p);
  }
};
#endif /* !WIN32 */

}  // namespace org::apache::nifi::minifi::utils
