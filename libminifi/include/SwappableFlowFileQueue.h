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

#include <future>
#include <memory>
#include <deque>
#include <chrono>

#include "utils/Id.h"
#include "utils/OptionalUtils.h"
#include "core/FlowFile.h"
#include "SwapManager.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class SwappableFlowFileQueue {
 public:
  SwappableFlowFileQueue() {

  }

  utils::optional<std::shared_ptr<core::FlowFile>> try_pop_front() {
    return try_pop_front(std::chrono::milliseconds{0});
  }

  template<typename Rep, typename Period>
  utils::optional<std::shared_ptr<core::FlowFile>> try_pop_front(std::chrono::duration<Rep, Period> timeout) {
    std::lock_guard<std::mutex> guard(mtx_);
    utils::optional<std::shared_ptr<core::FlowFile>> result;
    if (!head_.empty()) {
      result = std::move(head_.front());
      head_.pop_front();
      if (!load_task_ && head_.size() < head_size_threshold_) {
        logger_->log_debug("Head (%zu) shrunk below threshold (%zu), initiating load", head_.size(), head_size_threshold_);
        initiateLoad();
      }
      return result;
    }
    if (load_task_) {
      logger_->log_debug("Head is empty checking already running load task");
      auto status = load_task_.value().wait_for(timeout);
      if (status == std::future_status::timeout) {
        logger_->log_debug("Load task is not yet completed");
        return utils::nullopt;
      } else if (status == std::future_status::ready) {
        logger_->log_debug("Load task is completed");
        size_t count = 0;
        for (auto&& item : load_task_->get()) {
          ++count;
          head_.push_back(std::move(item));
        }
        load_task_.reset();
        logger_->log_debug("Swapped in '%zu' flow files", count);
        if (!head_.empty()) {
          // load provided items
          result = std::move(head_.front());
          head_.pop_front();
          if (head_.size() < head_size_threshold_) {
            logger_->log_debug("Head (%zu) shrunk below threshold (%zu), initiating load", head_.size(), head_size_threshold_);
            initiateLoad();
          }
          return result;
        }
      } else {
        throw std::logic_error("Deferred future?");
      }
    }
    if (!swapped_flow_files_.empty()) {
      logger_->log_debug("Head is empty, initiating load");
      initiateLoad();
      return utils::nullopt;
    }
    if (!tail_.empty()) {
      logger_->log_debug("Head is empty, no swapped-out flow files, dequeuing from tail");
      result = std::move(tail_.front());
      tail_.pop_front();
      return result;
    }
    logger_->log_debug("Queue is empty");
    return utils::nullopt;
  }

  void push_back(std::shared_ptr<core::FlowFile> flow_file) {
    std::lock_guard<std::mutex> guard(mtx_);
    tail_.push_back(std::move(flow_file));
    if (tail_.size() < tail_size_threshold_) {
      return;
    }
    logger_->log_debug("Tail (%zu) grew beyond threshold (%zu), load is initiated", tail_.size(), tail_size_threshold_);
    size_t flow_file_count = tail_.size() - tail_size_target_;
    if (swap_manager_->isStoreNoop()) {
      tail_.erase(tail_.begin(), tail_.begin() + flow_file_count);
      return;
    }
    std::vector<std::shared_ptr<core::FlowFile>> flow_files;
    flow_files.reserve(flow_file_count);
    for (size_t i = 0; i < flow_file_count; ++i) {
      flow_files.push_back(std::move(tail_.front()));
      tail_.pop_front();
    }
    swap_manager_->store(std::move(flow_files)).wait();
  }

  bool empty() const {
    std::lock_guard<std::mutex> guard(mtx_);
    return head_.empty() && !load_task_ && swapped_flow_files_.empty() && tail_.empty();
  }

 private:
  void initiateLoad() {
    if (load_task_) {
      throw std::logic_error("There is already an active load task running");
    }
    size_t flow_files_to_be_swapped_in = std::min(head_size_target_ - head_.size(), swapped_flow_files_.size());
    std::vector<utils::Identifier> flow_file_ids;
    flow_file_ids.reserve(flow_files_to_be_swapped_in);
    for (size_t i = 0; i < flow_files_to_be_swapped_in; ++i) {
      flow_file_ids.push_back(std::move(swapped_flow_files_.front()));
      swapped_flow_files_.pop_front();
    }
    load_task_ = swap_manager_->load(std::move(flow_file_ids));
  }

  std::shared_ptr<SwapManager> swap_manager_;

  mutable std::mutex mtx_;

  std::deque<std::shared_ptr<core::FlowFile>> tail_;
  std::deque<utils::Identifier> swapped_flow_files_;
  utils::optional<std::future<std::vector<std::shared_ptr<core::FlowFile>>>> load_task_;
  std::deque<std::shared_ptr<core::FlowFile>> head_;

  // a store is initiated if the tail_ grows beyond this threshold
  size_t tail_size_threshold_;
  // a given store will try to shrink the tail_ to this target
  size_t tail_size_target_;
  // a load is initiated if the head_ shrinks below this threshold
  size_t head_size_threshold_;
  // a given load will try to grow the head_ to this target
  size_t head_size_target_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
