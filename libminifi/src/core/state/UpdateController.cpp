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

#include "core/state/UpdateController.h"
#include <utility>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

UpdateStatus::UpdateStatus(UpdateState state, int16_t reason)
    : state_(state),
      reason_(reason) {
}

UpdateStatus::UpdateStatus(const UpdateStatus &other)
    : error_(other.error_),
      reason_(other.reason_),
      state_(other.state_) {
}

UpdateStatus::UpdateStatus(const UpdateStatus &&other)
    : error_(std::move(other.error_)),
      reason_(std::move(other.reason_)),
      state_(std::move(other.state_)) {
}

UpdateState UpdateStatus::getState() const {
  return state_;
}

std::string UpdateStatus::getError() const {
  return error_;
}

int16_t UpdateStatus::getReadonCode() const {
  return reason_;
}

UpdateStatus &UpdateStatus::operator=(const UpdateStatus &&other) {
  error_ = std::move(other.error_);
  reason_ = std::move(other.reason_);
  state_ = std::move(other.state_);
  return *this;
}

UpdateStatus &UpdateStatus::operator=(const UpdateStatus &other) {
  error_ = other.error_;
  reason_ = other.reason_;
  state_ = other.state_;
  return *this;
}

} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
