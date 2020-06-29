/**
 * @file ScopeGuard.h
 * Exception class declaration
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
#ifndef LIBMINIFI_INCLUDE_UTILS_SCOPEGUARD_H_
#define LIBMINIFI_INCLUDE_UTILS_SCOPEGUARD_H_

#include <utility>
#include <functional>

#include "gsl.h"
#include "core/Deprecated.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct ScopeGuard : ::gsl::final_action<std::function<void()>> {
  using ::gsl::final_action<std::function<void()>>::final_action;

  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 1.0) void disable() noexcept {
    dismiss();
  }
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_UTILS_SCOPEGUARD_H_
