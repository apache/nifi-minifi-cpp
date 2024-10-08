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
#ifndef LIBMINIFI_INCLUDE_UTILS_MINIFICONCURRENTQUEUE_H_
#define LIBMINIFI_INCLUDE_UTILS_MINIFICONCURRENTQUEUE_H_


#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <stdexcept>
#include <atomic>

#include "utils/TryMoveCall.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

// Provides a queue API and guarantees no race conditions in case of multiple producers and consumers.
// Guarantees elements to be dequeued in order of insertion
template <typename T>
class ConcurrentQueue {
 public:
  ConcurrentQueue() = default;

  ConcurrentQueue(const ConcurrentQueue& other) = delete;
  ConcurrentQueue& operator=(const ConcurrentQueue& other) = delete;
  ConcurrentQueue(ConcurrentQueue&& other)
    : ConcurrentQueue(std::move(other), std::lock_guard<std::mutex>(other.mtx_)) {}

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
    return tryDequeueImpl(lck, out);
  }

  template<typename Functor>
  bool consume(Functor&& fun) {
    std::unique_lock<std::mutex> lck(mtx_);
    return consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }

  bool empty() const {
    std::unique_lock<std::mutex> lck(mtx_);
    return emptyImpl(lck);
  }

  size_t size() const {
    std::lock_guard<std::mutex> guard(mtx_);
    return queue_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.clear();
  }

  template<typename Functor>
  void remove(Functor fun) {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(), fun), queue_.end());
  }

  template <typename... Args>
  void enqueue(Args&&... args) {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.emplace_back(std::forward<Args>(args)...);
  }

 private:
  ConcurrentQueue(ConcurrentQueue&& other, std::lock_guard<std::mutex>&)
    : queue_(std::move(other.queue_)) {}

 protected:
  void checkLock(std::unique_lock<std::mutex>& lck) const {
    if (!lck.owns_lock()) {
      throw std::logic_error("Caller of protected functions of ConcurrentQueue should own the lock!");
    }
  }

  // Warning: this function copies if T is not nothrow move constructible
  bool tryDequeueImpl(std::unique_lock<std::mutex>& lck, T& out) {
    checkLock(lck);
    if (queue_.empty()) {
      return false;
    }
    out = std::move_if_noexcept(queue_.front());
    queue_.pop_front();
    return true;
  }

  // Warning: this function copies if T is not nothrow move constructible
  template<typename Functor>
  bool consumeImpl(std::unique_lock<std::mutex>&& lock_to_adopt, Functor&& fun) {
    std::unique_lock<std::mutex> lock(std::move(lock_to_adopt));
    checkLock(lock);
    if (queue_.empty()) {
      return false;
    }
    T elem = std::move_if_noexcept(queue_.front());
    queue_.pop_front();
    lock.unlock();
    TryMoveCall<Functor, T>::call(std::forward<Functor>(fun), elem);
    return true;
  }

  bool emptyImpl(std::unique_lock<std::mutex>& lck) const {
    checkLock(lck);
    return queue_.empty();
  }

  mutable std::mutex mtx_;

 private:
  std::deque<T> queue_;
};


// A ConcurrentQueue extended with a condition variable to be able to block and wait for incoming data
// Stopping interrupts all consumers without a chance to consume remaining elements in the queue although elements can still be enqueued
// Started means queued elements can be consumed/dequeued and dequeueWait* calls can block
template <typename T>
class ConditionConcurrentQueue : private ConcurrentQueue<T> {
 public:
  explicit ConditionConcurrentQueue(bool start = true) : ConcurrentQueue<T>{}, running_{start} {}

  ConditionConcurrentQueue(const ConditionConcurrentQueue& other) = delete;
  ConditionConcurrentQueue& operator=(const ConditionConcurrentQueue& other) = delete;
  ConditionConcurrentQueue(ConditionConcurrentQueue&& other) = delete;
  ConditionConcurrentQueue& operator=(ConditionConcurrentQueue&& other) = delete;

  using ConcurrentQueue<T>::size;
  using ConcurrentQueue<T>::empty;
  using ConcurrentQueue<T>::clear;

  template <typename... Args>
  void enqueue(Args&&... args) {
    ConcurrentQueue<T>::enqueue(std::forward<Args>(args)...);
    if (running_) {
      cv_.notify_one();
    }
  }

  bool dequeueWait(T& out) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait(lck, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Only wake up if there is something to return or stopped
    return running_ && ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  template<typename Functor>
  bool consumeWait(Functor&& fun) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait(lck, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Only wake up if there is something to return or stopped
    return running_ && ConcurrentQueue<T>::consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }

  template< class Rep, class Period >
  bool dequeueWaitFor(T& out, const std::chrono::duration<Rep, Period>& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait_for(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return running_ && ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  bool dequeueWaitUntil(T& out, const std::chrono::system_clock::time_point& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait_until(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return running_ && ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  template<typename Functor, class Rep, class Period>
  bool consumeWaitFor(Functor&& fun, const std::chrono::duration<Rep, Period>& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait_for(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return running_ && ConcurrentQueue<T>::consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }

  template<typename Functor>
  bool consumeWaitUntil(Functor&& fun, const std::chrono::system_clock::time_point& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    cv_.wait_until(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return running_ && ConcurrentQueue<T>::consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }


  bool tryDequeue(T& out) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    return running_ && ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  void stop() {
    // this lock ensures that other threads did not yet
    // check the running_ condition (as they all acquire
    // the lock before the check) or already unlocked and
    // are waiting, thus receiving the notify_all
    // TODO(adebreceni): investigate a waiting_ counter
    //   approach that would render the locking here unnecessary
    std::lock_guard<std::mutex> guard(this->mtx_);
    running_ = false;
    cv_.notify_all();
  }

  void start() {
    running_ = true;
  }

  bool isRunning() const {
    return running_;  // In case it's not running no notifications are generated, dequeueing fails instead of blocking to avoid hanging threads
  }

  using ConcurrentQueue<T>::remove;

 private:
  std::atomic<bool> running_;
  std::condition_variable cv_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_MINIFICONCURRENTQUEUE_H_
