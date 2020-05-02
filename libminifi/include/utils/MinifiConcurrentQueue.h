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
#include <utility>
#include <stdexcept>
#include <type_traits>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace detail {
template<typename...>
using void_t = void;

// TryMoveCall calls an
//  - unary function of a lvalue reference-type argument by passing a ref
//  - unary function of any other argument type by moving into it
template<typename /* FunType */, typename T, typename = void>
struct TryMoveCall {
    template<typename Fun>
    static void call(Fun&& fun, T& elem) { std::forward<Fun>(fun)(elem); }
};

// 1.) std::declval looks similar to this: template<typename T> T&& declval();.
//     Not defined, therefore it's only usable in unevaluated context.
//     No requirements regarding T, therefore makes it possible to create hypothetical objects
//     without requiring e.g. a default constructor and a destructor, like the T{} expression does.
// 2.) std::declval<FunType>() resolves to an object of type FunType. If FunType is an lvalue reference,
//     then this will also result in an lvalue reference due to reference collapsing.
// 3.) std::declval<FunType>()(std::declval<T>()) resolves to an object of the result type of
//     a call on a function object of type FunType with an rvalue argument of type T.
//     It is ill-formed if the function object expect an lvalue reference.
//         - Example: FunType is a pointer to a bool(int) and T is int. This expression will result in a bool object.
//         - Example: FunType is a function object modeling bool(int&) and T is int. This expression will be ill-formed because it's illegal to bind an int rvalue to an int&.
// 4.) void_t<decltype(*3*)> checks for the well-formedness of 3., then discards it.
//     If 3. is ill-formed, then this specialization is ignored through SFINAE.
//     If well-formed, then it's considered more specialized than the other and takes precedence.
template<typename FunType, typename T>
struct TryMoveCall<FunType, T, void_t<decltype(std::declval<FunType>()(std::declval<T>()))>> {
    template<typename Fun>
    static void call(Fun&& fun, T& elem) { std::forward<Fun>(fun)(std::move(elem)); }
};
}  // namespace detail

// Provides a queue API and guarantees no race conditions in case of multiple producers and consumers.
// Guarantees elements to be dequeued in order of insertion
template <typename T>
class ConcurrentQueue {
 public:    
  explicit ConcurrentQueue() = default;

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

  template <typename... Args>
  void enqueue(Args&&... args) {
    std::lock_guard<std::mutex> guard(mtx_);
    queue_.emplace_back(std::forward<Args>(args)...);
  }

 private:
   ConcurrentQueue(ConcurrentQueue&& other, std::lock_guard<std::mutex>&)
    : queue_( std::move(other.queue_) ) {}

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
    detail::TryMoveCall<Functor, T>::call(std::forward<Functor>(fun), elem);
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
    if (!running_) {
      return false;
    }
    cv_.wait(lck, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Only wake up if there is something to return or stopped
    return ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  template<typename Functor>
  bool consumeWait(Functor&& fun) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    if (!running_) {
      return false;
    }
    cv_.wait(lck, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Only wake up if there is something to return or stopped
    return ConcurrentQueue<T>::consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }

  template< class Rep, class Period >
  bool dequeueWaitFor(T& out, const std::chrono::duration<Rep, Period>& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    if (!running_) {
      return false;
    }
    cv_.wait_for(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  template<typename Functor, class Rep, class Period>
  bool consumeWaitFor(Functor&& fun, const std::chrono::duration<Rep, Period>& time) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    if (!running_) {
      return false;
    }
    cv_.wait_for(lck, time, [this, &lck]{ return !running_ || !this->emptyImpl(lck); });  // Wake up with timeout or in case there is something to do
    return ConcurrentQueue<T>::consumeImpl(std::move(lck), std::forward<Functor>(fun));
  }

  bool tryDequeue(T& out) {
    std::unique_lock<std::mutex> lck(this->mtx_);
    return running_ && ConcurrentQueue<T>::tryDequeueImpl(lck, out);
  }

  void stop() {
    std::lock_guard<std::mutex> guard(this->mtx_);
    running_ = false;
    cv_.notify_all();
  }

  void start() {
    std::unique_lock<std::mutex> lck(this->mtx_);
    running_ = true;
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
