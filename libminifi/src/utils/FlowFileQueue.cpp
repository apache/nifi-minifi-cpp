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

#include "utils/FlowFileQueue.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

bool FlowFileQueue::FlowFilePenaltyExpirationComparator::operator()(const value_type& left, const value_type& right) {
  // this is operator< implemented using > so that top() is the element with the smallest key (earliest expiration)
  // rather than the element with the largest key, which is the default for std::priority_queue
  return left->getPenaltyExpiration() > right->getPenaltyExpiration();
}

FlowFileQueue::value_type FlowFileQueue::pop() {
  if (empty()) {
    throw std::logic_error{"pop() called on an empty FlowFileQueue"};
  }

  value_type next_flow_file = queue_.top();
  queue_.pop();
  return next_flow_file;
}

void FlowFileQueue::push(const value_type& element) {
  if (!element->isPenalized()) {
    element->penalize(std::chrono::milliseconds{0});
  }

  queue_.push(element);
}

void FlowFileQueue::push(value_type&& element) {
  if (!element->isPenalized()) {
    element->penalize(std::chrono::milliseconds{0});
  }

  queue_.push(std::move(element));
}

bool FlowFileQueue::isWorkAvailable() const {
  return !queue_.empty() && !queue_.top()->isPenalized();
}

bool FlowFileQueue::empty() const {
  return queue_.empty();
}

size_t FlowFileQueue::size() const {
  return queue_.size();
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
