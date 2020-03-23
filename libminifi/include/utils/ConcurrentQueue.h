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
#ifndef LIBMINIFI_INCLUDE_CONCURRENT_QUEUE_H
#define LIBMINIFI_INCLUDE_CONCURRENT_QUEUE_H

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template <typename T>
class ConcurrentQueue {
 public:    
  ConcurrentQueue() = default;
  virtual ~ConcurrentQueue() = default;

  ConcurrentQueue(const ConcurrentQueue& other) = delete;
  ConcurrentQueue& operator=(const ConcurrentQueue& other) = delete;
  ConcurrentQueue(ConcurrentQueue&& other)
    : ConcurrentQueue(std::move(other), std::lock_guard<std::mutex>(other.mutex_)) {}

  ConcurrentQueue& operator=(ConcurrentQueue&& other) {
    if (this != &other) {
      std::lock(mtx_, other.mtx_);
      std::lock_guard<std::mutex> lk1(mtx_, std::adopt_lock);
      std::lock_guard<std::mutex> lk2(other.mtx_, std::adopt_lock);
      queue_.swap(other.queue_);
    }
    return *this;
  }

  bool tryDequeue(T& out) {
    std::unique_lock<std::mutex> lck(mtx_);
    return tryDequeue(lck, out);
  }

  bool empty() const {
    std::lock_guard<std::mutex> guard(mtx_);
    return queue_.empty();
  }

  size_t size() const {
    std::lock_guard<std::mutex> guard(mtx_);
    return queue_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.clear();
  }

  template <typename... Args>
  void enqueue(Args&&... args) {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.emplace_back(std::forward<Args>(args)...);
  }

 private:
   ConcurrentQueue(ConcurrentQueue&& other, std::lock_guard<std::mutex>&)
    : queue_( std::move(other.queue_) ) {}

 protected:
  bool tryDequeue(std::unique_lock<std::mutex>& lck, T& out) {
    if (!lck.owns_lock()) {
      throw std::logic_error("Caller of protected ConcurrentQueue::tryDequeue should own the lock!");
    }
    if (queue_.empty()) {
      return false;
    }
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }
  std::deque<T> queue_;
  mutable std::mutex mtx_;
};

template <typename T>
class ConditionConcurrentQueue : public ConcurrentQueue<T> {
 public:
  ConditionConcurrentQueue(bool start = false) : ConcurrentQueue<T>(), running_{start} {};
  
  ConditionConcurrentQueue(const ConditionConcurrentQueue& other) = delete;
  ConditionConcurrentQueue& operator=(const ConditionConcurrentQueue& other) = delete;
  ConditionConcurrentQueue(ConditionConcurrentQueue&& other) = delete;
  ConditionConcurrentQueue& operator=(ConditionConcurrentQueue&& other) = delete;
  
  template <typename... Args>
  void enqueue(Args&&... args) {
    ConcurrentQueue<T>::enqueue(std::forward<Args>(args)...);
    if (running_) {
      cv_.notify_one();
    }
  }
  
  bool dequeue(T& out) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait(lck, [this]{ return !running_ || !this->queue_.empty(); });  // Only wake up if there is something to return or stopped 
    return running_ && ConcurrentQueue<T>::tryDequeue(lck, out);
  }
  
  void stop() {
    std::lock_guard<std::mutex> guard(this->mtx_);
    running_ = false;
    cv_.notify_all();
  }

  void start() {
    std::lock_guard<std::mutex> guard(this->mtx_);
    if (!running_) {
      running_ = true;
      if (!this->queue_.empty()) {
        cv_.notify_all();
      }
    }
  }
  
  bool isRunning() const {
    std::lock_guard<std::mutex> guard(this->mtx_);
    return running_;  // In case it's not running no notifications are generated, dequeueing fails instead of blocking to avoid hanging threads
  }

 private:
  bool running_;
  std::condition_variable cv_;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_CONCURRENT_QUEUE_H
