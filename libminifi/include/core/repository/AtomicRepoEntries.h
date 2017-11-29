/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ref_count_hip.
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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_ATOMICREPOENTRIES_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_ATOMICREPOENTRIES_H_

#include  <cstddef>
#include <cstring>
#include <iostream>
#include <chrono>
#include <functional>
#include <atomic>
#include <vector>
#include <map>
#include <iterator>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * Purpose: Repo value represents an item that will support a move operation within an AtomicEntry
 *
 * Justification: Since AtomicEntry is a static entry that does not move or change, the underlying
 * RepoValue can be changed to support atomic operations.
 */
template<typename T>
class RepoValue {
 public:

  explicit RepoValue() {
  }

  /**
   * Constructor that populates the item allowing for a custom key comparator.
   * @param key key for this repo value.
   * @param ptr buffer
   * @param size size buffer
   * @param comparator custom comparator.
   */
  explicit RepoValue(T key, const uint8_t *ptr, size_t size, std::function<bool(T, T)> comparator = nullptr)
      : key_(key),
        comparator_(comparator) {
    if (nullptr == ptr) {
      size = 0;
    }
    buffer_.resize(size);
    if (size > 0) {
      std::memcpy(buffer_.data(), ptr, size);
    }
  }

  /**
   * RepoValue that moves the other object into this.
   */
  explicit RepoValue(RepoValue<T> &&other)
noexcept      : key_(std::move(other.key_)),
      buffer_(std::move(other.buffer_)),
      comparator_(std::move(other.comparator_)) {
      }

      ~RepoValue()
      {
      }

      T &getKey() {
        return key_;
      }

      /**
       * Sets the key, relacing the custom comparator if needed.
       */
      void setKey(const T key, std::function<bool(T,T)> comparator = nullptr) {
        key_ = key;
        comparator_ = comparator;
      }

      /**
       * Determines if the key is the same using the custom comparator
       * @param other object to compare against
       * @return result of the comparison
       */
      inline bool isEqual(RepoValue<T> *other)
      {
        return comparator_ == nullptr ? key_ == other->key_ : comparator_(key_,other->key_);
      }

      /**
       * Determines if the key is the same using the custom comparator
       * @param other object to compare against
       * @return result of the comparison
       */
      inline bool isKey(T other)
      {
        return comparator_ == nullptr ? key_ == other : comparator_(key_,other);
      }

      /**
       * Clears the buffer.
       */
      void clearBuffer() {
        buffer_.resize(0);
        buffer_.clear();
      }

      /**
       * Return the size of the memory within the key
       * buffer, the size of timestamp, and the general
       * system word size
       */
      uint64_t size() {
        return buffer_.size();
      }

      size_t getBufferSize() {
        return buffer_.size();
      }

      const uint8_t *getBuffer()
      {
        return buffer_.data();
      }

      /**
       * Places the contents of buffer into str
       * @param strnig into which we are placing the memory contained in buffer.
       */
      void emplace(std::string &str) {
        str.insert(0, reinterpret_cast<const char*>(buffer_.data()), buffer_.size());
      }

      /**
       * Appends ptr to the end of buffer.
       * @param ptr pointer containing data to add to buffer_
       */
      void append(uint8_t *ptr, size_t size)
      {
        buffer_.insert(buffer_.end(), ptr, ptr + size);
      }

      RepoValue<T> &operator=(RepoValue<T> &&other) noexcept {
        key_ = std::move(other.key_);
        buffer_ = std::move(other.buffer_);
        return *this;
      }

    private:
      T key_;
      std::function<bool(T,T)> comparator_;
      std::vector<uint8_t> buffer_;
    };

    /**
     * Purpose: Atomic Entry allows us to create a statically
     * sized ring buffer, with the ability to create
     *
     **/
template<typename T>
class AtomicEntry {

 public:
  /**
   * Constructor that accepts a max size and an atomic counter for the total
   * size allowd by this and other atomic entries.
   */
  explicit AtomicEntry(std::atomic<size_t> *total_size, size_t *max_size)
      : accumulated_repo_size_(total_size),
        max_repo_size_(max_size),
        write_pending_(false),
        has_value_(false),
        ref_count_(0),
        free_required(false) {
  }

  /**
   * Sets the repo value, moving the old value into old_value.
   * @param new_value new value to move into value_.
   * @param old_value the previous value of value_ will be moved into old_value
   * @param prev_size size reclaimed.
   * @return result of this set. If true old_value will be populated.
   */
  bool setRepoValue(RepoValue<T> &new_value, RepoValue<T> &old_value, size_t &prev_size) {
    // delete the underlying pointer
    bool lock = false;
    if (!write_pending_.compare_exchange_weak(lock, true)) {
      return false;
    }
    if (has_value_) {
      prev_size = value_.size();
    }
    old_value = std::move(value_);
    value_ = std::move(new_value);
    has_value_ = true;
    try_unlock();
    return true;
  }

  AtomicEntry<T> *takeOwnership() {
    bool lock = false;
    if (!write_pending_.compare_exchange_weak(lock, true))
      return nullptr;

    ref_count_++;

    try_unlock();

    return this;
  }
  /**
   * A test and set operation, which is used to allow a function to test
   * if an item can be released and a function used for reclaiming memory associated
   * with said object.
   * A custom comparator can be provided to augment the key being added into value_
   */
  bool testAndSetKey(const T str, std::function<bool(T)> releaseTest = nullptr, std::function<void(T)> reclaimer = nullptr, std::function<bool(T, T)> comparator = nullptr) {
    bool lock = false;

    if (!write_pending_.compare_exchange_weak(lock, true))
      return false;

    if (has_value_) {
      // we either don't have a release test or we cannot release this
      // entity
      if (releaseTest != nullptr && reclaimer != nullptr && releaseTest(value_.getKey())) {
        reclaimer(value_.getKey());
      } else if (free_required && ref_count_ == 0) {
        size_t bufferSize = value_.getBufferSize();
        value_.clearBuffer();
        has_value_ = false;
        if (accumulated_repo_size_ != nullptr) {
          *accumulated_repo_size_ -= bufferSize;
        }
        free_required = false;
      } else {
        try_unlock();
        return false;
      }

    }
    ref_count_ = 1;
    value_.setKey(str, comparator);
    has_value_ = true;
    try_unlock();
    return true;
  }

  /**
   * Moved the value into the argument
   * @param value the previous value will be moved into this parameter
   * @return  success of get operation based on whether or not this atomic entry has a value.
   */
  bool getValue(RepoValue<T> &value) {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return false;
    }
    value = std::move(value_);
    has_value_ = false;
    try_unlock();
    return true;
  }

  /**
   * Moved the value into the argument
   * @param value the previous value will be moved into this parameter
   * @return  success of get operation based on whether or not this atomic entry has a value.
   */
  bool getValue(const T &key, RepoValue<T> &value) {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return false;
    }
    if (!value_.isKey(key)) {
      try_unlock();
      return false;
    }
    value = std::move(value_);
    has_value_ = false;
    try_unlock();
    return true;
  }

  void decrementOwnership() {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return;
    }
    if (ref_count_ > 0) {
      ref_count_--;
    }
    if (ref_count_ == 0 && free_required) {
      size_t bufferSize = value_.getBufferSize();
      value_.clearBuffer();
      has_value_ = false;
      if (accumulated_repo_size_ != nullptr) {
        *accumulated_repo_size_ -= bufferSize;
      }
      free_required = false;
    } else {
    }
    try_unlock();
  }

  /**
   * Moved the value into the argument
   * @param value the previous value will be moved into this parameter
   * @return  success of get operation based on whether or not this atomic entry has a value.
   */
  bool getValue(const T &key, RepoValue<T> **value) {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return false;
    }
    if (!value_.isKey(key)) {
      try_unlock();
      return false;
    }
    ref_count_++;
    *value = &value_;
    try_unlock();
    return true;
  }

  /**
   * Operation that will be used to test and free if a release is required without
   * setting a new object.
   * @param releaseTest function that will be used to free the RepoValue key from
   * this atomic entry.
   * @param freedValue informs the caller if an item was freed.
   */
  T testAndFree(std::function<bool(T)> releaseTest, bool &freedValue) {
    try_lock();
    T ref;
    if (!has_value_) {
      try_unlock();
      return ref;
    }

    if (releaseTest(value_.getKey())) {
      size_t bufferSize = value_.getBufferSize();
      value_.clearBuffer();
      ref = value_.getKey();
      has_value_ = false;
      if (accumulated_repo_size_ != nullptr) {
        *accumulated_repo_size_ -= bufferSize;
      }

    }
    try_unlock();
    return ref;
  }

  size_t getLength() {
    size_t size = 0;
    try_lock();
    size = value_.getBufferSize();
    try_unlock();
    return size;

  }

  /**
   * sets has_value to false; however, does not call
   * any external entity to further free RepoValue
   */
  bool freeValue(const T &key) {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return false;
    }
    if (!value_.isKey(key)) {
      try_unlock();
      return false;
    }
    if (ref_count_ > 0) {
      free_required = true;
      try_unlock();
      return true;
    }
    size_t bufferSize = value_.getBufferSize();
    value_.clearBuffer();
    has_value_ = false;
    if (accumulated_repo_size_ != nullptr) {
      *accumulated_repo_size_ -= bufferSize;
    }
    free_required = false;
    try_unlock();
    return true;
  }

  /**
   * Appends buffer onto this atomic entry if key matches
   * the current RepoValue's key.
   */
  bool insert(const T key, uint8_t *buffer, size_t size) {
    try_lock();

    if (!has_value_) {
      try_unlock();
      return false;
    }

    if (!value_.isKey(key)) {
      try_unlock();
      return false;
    }

    if ((accumulated_repo_size_ != nullptr && max_repo_size_ != nullptr) && (*accumulated_repo_size_ + size > *max_repo_size_)) {
      // can't support this write
      try_unlock();
      return false;
    }

    value_.append(buffer, size);
    (*accumulated_repo_size_) += size;
    try_unlock();
    return true;
  }

 private:

  /**
   * Spin lock to unlock the current atomic entry.
   */
  inline void try_lock() {
    bool lock = false;
    while (!write_pending_.compare_exchange_weak(lock, true, std::memory_order_acquire)) {
      lock = false;
      // attempt again
    }
  }

  /**
   * Spin lock to unlock the current atomic entry.
   */
  inline void try_unlock() {
    bool lock = true;
    while (!write_pending_.compare_exchange_weak(lock, false, std::memory_order_acquire)) {
      lock = true;
      // attempt again
    }
  }

  // atomic size pointer.
  std::atomic<size_t> *accumulated_repo_size_;
  // max size
  size_t *max_repo_size_;
  // determines if a write is pending.
  std::atomic<bool> write_pending_;
  // used to determine if a value is present in this atomic entry.
  std::atomic<bool> has_value_;
  std::atomic<uint16_t> ref_count_;
  std::atomic<bool> free_required;
  // repo value.
  RepoValue<T> value_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_ATOMICREPOENTRIES_H_ */
