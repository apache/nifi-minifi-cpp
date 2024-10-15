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

#include <mutex>
#include <atomic>
#include <utility>
#include "MinifiConcurrentQueue.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace internal {
template<typename T>
struct default_allocator {
  T operator()(size_t max_size) const {
    return T::allocate(max_size);
  }
};
}  // namespace internal

/**
 * Purpose: A FIFO container that allows chunked processing while trying to enforce
 * soft limits like max chunk size and max total size. The "head" chunk might be
 * modified in a thread-safe manner (usually appending to it) before committing it
 * thus making it available for dequeuing.
 */
template<typename ActiveItem, typename Allocator = internal::default_allocator<ActiveItem>>
class StagingQueue {
  using Item = typename std::decay<decltype(std::declval<ActiveItem&>().commit())>::type;

  static_assert(std::is_same<decltype(std::declval<const Allocator&>()(std::declval<size_t>())), ActiveItem>::value,
      "Allocator::operator(size_t) must return an ActiveItem");
  static_assert(std::is_same<decltype(std::declval<const Item&>().size()), size_t>::value,
      "Item::size must return size_t");
  static_assert(std::is_same<decltype(std::declval<const ActiveItem&>().size()), size_t>::value,
      "ActiveItem::size must return size_t");

  template<typename Functor, typename Arg, typename = void>
  struct FunctorCallHelper;

  template<typename Functor, typename Arg>
  struct FunctorCallHelper<Functor, Arg, typename std::enable_if<std::is_same<decltype(std::declval<Functor>()(std::declval<Arg>())), bool>::value>::type> {
    static bool call(Functor&& fn, Arg&& arg) {
      return std::forward<Functor>(fn)(std::forward<Arg>(arg));
    }
  };

  template<typename Functor, typename Arg>
  struct FunctorCallHelper<Functor, Arg, typename std::enable_if<std::is_same<decltype(std::declval<Functor>()(std::declval<Arg>())), void>::value>::type> {
    static bool call(Functor&& fn, Arg&& arg) {
      std::forward<Functor>(fn)(std::forward<Arg>(arg));
      return false;
    }
  };

  static ActiveItem allocateActiveItem(const Allocator& allocator, size_t max_item_size) {
    // max_size is a soft limit, i.e. reaching max_size is an indicator
    // that that item should be committed, we cannot guarantee that only
    // max_size content is in the item, since max_size is the "trigger limit",
    // presumable each item would contain (at the trigger point) a little
    // more than max_size content, that is the reasoning behind "* 3 / 2"
    return allocator(max_item_size * 3 / 2);
  }

 public:
  StagingQueue(size_t max_size, size_t max_item_size, Allocator allocator = {})
    : max_size_(max_size),
      max_item_size_(max_item_size),
      active_item_(allocateActiveItem(allocator, max_item_size)),
      allocator_(allocator) {}

  void commit() {
    std::unique_lock<std::mutex> lock{active_item_mutex_};
    if (active_item_.size() == 0) {
      // nothing to commit
      return;
    }
    commit(lock);
  }

  /**
   * Allows thread-safe modification of the "live" instance.
   * @tparam Functor
   * @param fn callable which can modify the instance, should return true
   * if it would like to force a commit
   */
  template<typename Functor>
  void modify(Functor&& fn) {
    std::unique_lock<std::mutex> lock{active_item_mutex_};
    size_t original_size = active_item_.size();
    bool should_commit = FunctorCallHelper<Functor, ActiveItem&>::call(std::forward<Functor>(fn), active_item_);
    size_t new_size = active_item_.size();
    if (new_size >= original_size) {
      total_size_ += new_size - original_size;
    } else {
      total_size_ -= original_size - new_size;
    }
    if (should_commit || new_size > max_item_size_) {
      commit(lock);
    }
  }

  template<class Rep, class Period>
  bool tryDequeue(Item& out, const std::chrono::duration<Rep, Period>& time) {
    if (time == std::chrono::duration<Rep, Period>{0}) {
      return tryDequeue(out);
    }
    if (queue_.dequeueWaitFor(out, time)) {
      total_size_ -= out.size();
      return true;
    }
    return false;
  }

  bool tryDequeue(Item& out) {
    if (queue_.tryDequeue(out)) {
      total_size_ -= out.size();
      return true;
    }
    return false;
  }

  size_t getMaxSize() const {
    return max_size_;
  }

  size_t getMaxItemSize() const {
    return max_item_size_;
  }

  void discardOverflow() {
    while (total_size_ > max_size_) {
      Item item;
      if (!queue_.tryDequeue(item)) {
        break;
      }
      total_size_ -= item.size();
    }
  }

  size_t size() const {
    return total_size_;
  }

  size_t itemCount() const {
    return queue_.size();
  }

 private:
  void commit(std::unique_lock<std::mutex>& /*lock*/) {
    queue_.enqueue(active_item_.commit());
    active_item_ = allocateActiveItem(allocator_, max_item_size_);
  }

  const size_t max_size_;
  const size_t max_item_size_;
  std::atomic<size_t> total_size_{0};

  std::mutex active_item_mutex_;
  ActiveItem active_item_;

  const Allocator allocator_;

  ConditionConcurrentQueue<Item> queue_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
