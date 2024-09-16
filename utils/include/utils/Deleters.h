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
#ifndef LIBMINIFI_INCLUDE_UTILS_DELETERS_H_
#define LIBMINIFI_INCLUDE_UTILS_DELETERS_H_

#include <cstdlib>
#ifdef WIN32
#include <WS2tcpip.h>
#else
#include <ifaddrs.h>
#endif /* WIN32 */
#include <memory>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

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

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_UTILS_DELETERS_H_
