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
  if (existsFlowFileWithExpiredPenalty()) {
    value_type next_flow_file = priority_queue_.top();
    priority_queue_.pop();
    return next_flow_file;
  }

  if (!fifo_queue_.empty()) {
    value_type next_flow_file = fifo_queue_.front();
    fifo_queue_.pop();
    return next_flow_file;
  }

  throw std::logic_error{"pop() called on FlowFileQueue when canBePopped() is false"};
}

/**
 * Pops any flow file off the queue, whether it has an unexpired penalty or not.
 */
FlowFileQueue::value_type FlowFileQueue::forcePop() {
  if (!fifo_queue_.empty()) {
    value_type next_flow_file = fifo_queue_.front();
    fifo_queue_.pop();
    return next_flow_file;
  }

  if (!priority_queue_.empty()) {
    value_type next_flow_file = priority_queue_.top();
    priority_queue_.pop();
    return next_flow_file;
  }

  throw std::logic_error{"forcePop() called on an empty FlowFileQueue"};
}

void FlowFileQueue::push(const value_type& element) {
  if (element->isPenalized()) {
    priority_queue_.push(element);
  } else {
    fifo_queue_.push(element);
  }
}

void FlowFileQueue::push(value_type&& element) {
  if (element->isPenalized()) {
    priority_queue_.push(std::move(element));
  } else {
    fifo_queue_.push(std::move(element));
  }
}

bool FlowFileQueue::canBePopped() const {
  return !fifo_queue_.empty() || existsFlowFileWithExpiredPenalty();
}

bool FlowFileQueue::empty() const {
  return fifo_queue_.empty() && priority_queue_.empty();
}

size_t FlowFileQueue::size() const {
  return fifo_queue_.size() + priority_queue_.size();
}

bool FlowFileQueue::existsFlowFileWithExpiredPenalty() const {
  return !priority_queue_.empty() && !priority_queue_.top()->isPenalized();
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
