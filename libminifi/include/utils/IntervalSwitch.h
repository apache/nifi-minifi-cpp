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

#include <functional>
#include <utility>

#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

enum class IntervalSwitchState {
  LOWER,
  UPPER,
};

namespace detail {
struct SwitchReturn {
  IntervalSwitchState state;
  bool switched;
};
}  // namespace detail

template<typename T, typename Comp = std::less<T>>
class IntervalSwitch {
 public:
  IntervalSwitch(T lower_threshold, T upper_threshold, const IntervalSwitchState initial_state = IntervalSwitchState::UPPER)
      :lower_threshold_{std::move(lower_threshold)}, upper_threshold_{std::move(upper_threshold)}, state_{initial_state} {
    gsl_Expects(!less_(upper_threshold_, lower_threshold_));
  }

  detail::SwitchReturn operator()(const T& value) {
    const auto old_state = state_;
    if (less_(value, lower_threshold_)) {
      state_ = IntervalSwitchState::LOWER;
    } else if (!less_(value, upper_threshold_)) {
      state_ = IntervalSwitchState::UPPER;
    }
    return {state_, state_ != old_state};
  }

 private:
  T lower_threshold_;
  T upper_threshold_;
  Comp less_;

  IntervalSwitchState state_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
