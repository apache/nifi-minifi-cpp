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
#ifndef __SCOPE_GUARD_H__
#define __SCOPE_GUARD_H__

#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class ScopeGuard {
 private:
  bool enabled_;
  std::function<void()> func_;

 public:
  ScopeGuard(std::function<void()>&& func)
    : enabled_(true)
    , func_(std::move(func)) {
  }

  ~ScopeGuard() {
    if (enabled_) {
      try {
        func_();
      } catch (...) {
      }
    }
  }

  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard(ScopeGuard&&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;

  void disable() {
    enabled_ = false;
  }
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
