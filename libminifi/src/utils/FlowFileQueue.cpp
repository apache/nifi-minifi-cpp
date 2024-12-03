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
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils {

bool FlowFileQueue::FlowFilePenaltyExpirationComparator::operator()(const value_type& left, const value_type& right) const {
  // a flow file with earlier expiration compares less
  return left->getPenaltyExpiration() < right->getPenaltyExpiration();
}

bool FlowFileQueue::SwappedFlowFileComparator::operator()(const SwappedFlowFile& left, const SwappedFlowFile& right) const {
  // a swapped flow file with earlier expiration compares less
  return left.to_be_processed_after < right.to_be_processed_after;
}

FlowFileQueue::FlowFileQueue(std::shared_ptr<SwapManager> swap_manager)
  : swap_manager_(std::move(swap_manager)),
    logger_(core::logging::LoggerFactory<FlowFileQueue>::getLogger()) {}

FlowFileQueue::value_type FlowFileQueue::pop() {
  return tryPopImpl({}).value();
}

std::optional<FlowFileQueue::value_type> FlowFileQueue::tryPop() {
  return tryPopImpl(std::chrono::milliseconds{0});
}

std::optional<FlowFileQueue::value_type> FlowFileQueue::tryPop(std::chrono::milliseconds timeout) {
  return tryPopImpl(timeout);
}

std::optional<FlowFileQueue::value_type> FlowFileQueue::tryPopImpl(std::optional<std::chrono::milliseconds> timeout) {
  std::optional<std::shared_ptr<core::FlowFile>> result;
  if (!queue_.empty()) {
    result = queue_.popMin();
    if (processLoadTaskWait(std::chrono::milliseconds{0})) {
      initiateLoadIfNeeded();
    }
    return result;
  }
  if (load_task_) {
    logger_->log_debug("Head is empty checking already running load task");
    if (!processLoadTaskWait(timeout)) {
      return std::nullopt;
    }
    if (!queue_.empty()) {
      // load provided items
      result = queue_.popMin();
      initiateLoadIfNeeded();
      return result;
    }
  }
  // no pending load_task_ and no items in the queue_
  initiateLoadIfNeeded();
  return std::nullopt;
}

bool FlowFileQueue::processLoadTaskWait(std::optional<std::chrono::milliseconds> timeout) {
  if (!load_task_) {
    return true;
  }
  std::future_status status = std::future_status::ready;
  if (timeout) {
    status = load_task_.value().items.wait_for(timeout.value());
  }
  if (status == std::future_status::timeout) {
    logger_->log_debug("Load task is not yet completed");
    return false;
  }
  gsl_Assert(status == std::future_status::ready);

  logger_->log_debug("Getting loaded flow files");
  size_t swapped_in_count = 0;
  size_t intermediate_count = 0;
  for (auto&& item : load_task_->items.get()) {
    ++swapped_in_count;
    queue_.push(std::move(item));
  }
  for (auto&& intermediate_item : load_task_->intermediate_items) {
    ++intermediate_count;
    queue_.push(std::move(intermediate_item));
  }
  load_task_.reset();
  logger_->log_debug("Swapped in '{}' flow files and committed '{}' pending files", swapped_in_count, intermediate_count);
  return true;
}

void FlowFileQueue::push(value_type element) {
  // do not allow pushing elements in the past
  element->setPenaltyExpiration(std::max(element->getPenaltyExpiration(), clock_->now()));

  std::vector<value_type> flow_files_to_be_swapped_out;

  if (load_task_) {
    if (element->getPenaltyExpiration() <= load_task_->min) {
      // flow file goes before load_task_
      queue_.push(std::move(element));
    } else if (load_task_->max <= element->getPenaltyExpiration()) {
      // flow file goes after load_task_, i.e. immediately swapped out
      flow_files_to_be_swapped_out.push_back(std::move(element));
    } else {
      // flow file belongs to the same range that is being swapped in
      load_task_->intermediate_items.push_back(std::move(element));
    }
  } else if (!swapped_flow_files_.empty() && swapped_flow_files_.min().to_be_processed_after < element->getPenaltyExpiration()) {
    // flow file goes into the swapped_flow_files_ set, i.e. immediately swapped out
    flow_files_to_be_swapped_out.push_back(std::move(element));
  } else {
    queue_.push(std::move(element));
  }

  size_t flow_file_count = shouldSwapOutCount();
  if (flow_file_count != 0) {
    if (!load_task_) {
      // we cannot initiate a queue_ swap while a load_task_ is pending
      flow_files_to_be_swapped_out.reserve(flow_files_to_be_swapped_out.size() + flow_file_count);
      for (size_t i = 0; i < flow_file_count; ++i) {
        flow_files_to_be_swapped_out.push_back(queue_.popMax());
      }
    }
  }
  if (!flow_files_to_be_swapped_out.empty()) {
    for (const auto& flow_file : flow_files_to_be_swapped_out) {
      swapped_flow_files_.push(SwappedFlowFile{flow_file->getUUID(), flow_file->getPenaltyExpiration()});
    }
    logger_->log_debug("Initiating store of {} flow files", flow_files_to_be_swapped_out.size());
    swap_manager_->store(std::move(flow_files_to_be_swapped_out));
  }
}

bool FlowFileQueue::isWorkAvailable() const {
  auto now = clock_->now();
  if (!queue_.empty()) {
    return queue_.min()->getPenaltyExpiration() <= now;
  }
  if (load_task_) {
    if (load_task_->min > now) {
      return false;
    }
    auto status = load_task_->items.wait_for(std::chrono::milliseconds{0});
    return status == std::future_status::ready;
  }
  return !swapped_flow_files_.empty() && swapped_flow_files_.min().to_be_processed_after <= now;
}

bool FlowFileQueue::empty() const {
  return size() == 0;
}

size_t FlowFileQueue::size() const {
  return queue_.size() + (load_task_ ? load_task_->size()  : 0) + swapped_flow_files_.size();
}

void FlowFileQueue::clear() {
  queue_.clear();
  load_task_.reset();
  swapped_flow_files_.clear();
}

void FlowFileQueue::initiateLoadIfNeeded() {
  if (load_task_) {
    throw std::logic_error("There is already an active load task running");
  }
  size_t flow_files_count = shouldSwapInCount();
  if (flow_files_count == 0) {
    return;
  }
  logger_->log_debug("Initiating load of {} flow files", flow_files_count);
  TimePoint min = TimePoint::max();
  TimePoint max = TimePoint::min();
  std::vector<SwappedFlowFile> flow_files;
  flow_files.reserve(flow_files_count);
  for (size_t i = 0; i < flow_files_count; ++i) {
    SwappedFlowFile flow_file = swapped_flow_files_.popMin();
    // TODO(adebreceni): since we are popping in order, we could elide these std::min and std::max comparisons
    min = std::min(min, flow_file.to_be_processed_after);
    max = std::max(max, flow_file.to_be_processed_after);
    flow_files.push_back(flow_file);
  }
  load_task_ = {min, max, swap_manager_->load(std::move(flow_files)), flow_files_count};
}

void FlowFileQueue::setMinSize(size_t min_size) {
  min_size_ = min_size;
}

void FlowFileQueue::setTargetSize(size_t target_size) {
  target_size_ = target_size;
}

void FlowFileQueue::setMaxSize(size_t max_size) {
  max_size_ = max_size;
}

size_t FlowFileQueue::shouldSwapOutCount() const {
  if (!swap_manager_) {
    return 0;
  }
  // read once for consistent view of a single atomic variable
  size_t max_size = max_size_;
  size_t target_size = target_size_;
  if (max_size != 0 && target_size != 0
      && max_size < queue_.size() && target_size < queue_.size()) {
    return queue_.size() - target_size;
  }
  return 0;
}

size_t FlowFileQueue::shouldSwapInCount() const {
  if (!swap_manager_) {
    return 0;
  }
  // read once for consistent view of a single atomic variable
  size_t min_size = min_size_;
  size_t target_size = target_size_;
  if (min_size == 0 || target_size == 0) {
    if (!swapped_flow_files_.empty()) {
      logger_->log_info("Swapping in all the flow files");
      return swapped_flow_files_.size();
    }
    return 0;
  }
  if (queue_.size() < min_size && queue_.size() < target_size) {
    return std::min(target_size - queue_.size(), swapped_flow_files_.size());
  }
  return 0;
}


}  // namespace org::apache::nifi::minifi::utils
