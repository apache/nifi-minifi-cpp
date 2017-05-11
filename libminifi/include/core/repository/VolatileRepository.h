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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileRepository_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileRepository_H_

#include "core/Repository.h"
#include <chrono>
#include <vector>
#include <map>
#include "core/Core.h"
#include "Connection.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

static uint16_t accounting_size = sizeof(std::vector<uint8_t>)
    + sizeof(std::string) + sizeof(size_t);

class RepoValue {
 public:

  explicit RepoValue() {
  }

  explicit RepoValue(std::string key, uint8_t *ptr, size_t size)
      : key_(key) {
    buffer_.resize(size);
    std::memcpy(buffer_.data(), ptr, size);
    fast_size_ = key.size() + size;
  }

  explicit RepoValue(RepoValue &&other)
noexcept      : key_(std::move(other.key_)),
      buffer_(std::move(other.buffer_)),
      fast_size_(other.fast_size_) {
      }

      ~RepoValue()
      {
      }

      std::string &getKey() {
        return key_;
      }

      /**
       * Return the size of the memory within the key
       * buffer, the size of timestamp, and the general
       * system word size
       */
      uint64_t size() {
        return fast_size_;
      }

      size_t bufferSize() {
        return buffer_.size();
      }

      void emplace(std::string &str) {
        str.insert(0, reinterpret_cast<const char*>(buffer_.data()), buffer_.size());
      }

      RepoValue &operator=(RepoValue &&other) noexcept {
        key_ = std::move(other.key_);
        buffer_ = std::move(other.buffer_);
        other.buffer_.clear();
        return *this;
      }

    private:
      size_t fast_size_;
      std::string key_;
      std::vector<uint8_t> buffer_;
    };

    /**
     * Purpose: Atomic Entry allows us to create a statically
     * sized ring buffer, with the ability to create
     **/
class AtomicEntry {

 public:
  AtomicEntry()
      : write_pending_(false),
        has_value_(false) {

  }

  bool setRepoValue(RepoValue &new_value, size_t &prev_size) {
    // delete the underlying pointer
    bool lock = false;
    if (!write_pending_.compare_exchange_weak(lock, true) && !lock)
      return false;
    if (has_value_) {
      prev_size = value_.size();
    }
    value_ = std::move(new_value);
    has_value_ = true;
    try_unlock();
    return true;
  }

  bool getValue(RepoValue &value) {
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

  bool getValue(const std::string &key, RepoValue &value) {
    try_lock();
    if (!has_value_) {
      try_unlock();
      return false;
    }
    if (value_.getKey() != key) {
      try_unlock();
      return false;
    }
    value = std::move(value_);
    has_value_ = false;
    try_unlock();
    return true;
  }

 private:

  inline void try_lock() {
    bool lock = false;
    while (!write_pending_.compare_exchange_weak(lock, true) && !lock) {
      // attempt again
    }
  }

  inline void try_unlock() {
    bool lock = true;
    while (!write_pending_.compare_exchange_weak(lock, false) && lock) {
      // attempt again
    }
  }

  std::atomic<bool> write_pending_;
  std::atomic<bool> has_value_;
  RepoValue value_;
};

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using LevelDB as the primary substrate.
 */
class VolatileRepository : public core::Repository,
    public std::enable_shared_from_this<VolatileRepository> {
 public:

  static const char *volatile_repo_max_count;
  // Constructor

  VolatileRepository(
      std::string repo_name = "", std::string dir = REPOSITORY_DIRECTORY,
      int64_t maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
      int64_t maxPartitionBytes =
      MAX_REPOSITORY_STORAGE_SIZE,
      uint64_t purgePeriod = REPOSITORY_PURGE_PERIOD)
      : Repository(
            repo_name.length() > 0 ?
                repo_name : core::getClassName<VolatileRepository>(),
            "", maxPartitionMillis, maxPartitionBytes, purgePeriod),
        max_size_(maxPartitionBytes * 0.75),
        current_index_(0),
        max_count_(10000),
        logger_(logging::LoggerFactory<VolatileRepository>::getLogger())

  {

  }

  // Destructor
  ~VolatileRepository() {
    for (auto ent : value_vector_) {
      delete ent;
    }
  }

  /**
   * Initialize thevolatile repsitory
   **/
  virtual bool initialize(const std::shared_ptr<Configure> &configure) {
    std::string value = "";

    if (configure != nullptr) {
      int64_t max_cnt = 0;
      std::stringstream strstream;
      strstream << Configure::nifi_volatile_repository_options << getName()
                << "." << volatile_repo_max_count;
      if (configure->get(strstream.str(), value)) {
        if (core::Property::StringToInt(value, max_cnt)) {
          max_count_ = max_cnt;
        }

      }
    }

    logger_->log_debug("Resizing value_vector_ for %s count is %d", getName(),
                       max_count_);
    value_vector_.reserve(max_count_);
    for (int i = 0; i < max_count_; i++) {
      value_vector_.emplace_back(new AtomicEntry());
    }
    return true;
  }

  virtual void run();

  /**
   * Places a new object into the volatile memory area
   * @param key key to add to the repository
   * @param buf buffer 
   **/
  virtual bool Put(std::string key, uint8_t *buf, int bufLen) {
    RepoValue new_value(key, buf, bufLen);

    const size_t size = new_value.size();
    bool updated = false;
    size_t reclaimed_size = 0;
    do {

      int private_index = current_index_.fetch_add(1);
      // round robin through the beginning
      if (private_index >= max_count_) {
        uint16_t new_index = 0;
        if (current_index_.compare_exchange_weak(new_index, 0)) {
          private_index = 0;
        } else {
          continue;
        }
      }
      logger_->log_info("Set repo value at %d out of %d", private_index,
                        max_count_);
      updated = value_vector_.at(private_index)->setRepoValue(new_value,
                                                              reclaimed_size);

      if (reclaimed_size > 0) {
        current_size_ -= reclaimed_size;
      }

    } while (!updated);
    current_size_ += size;

    logger_->log_info("VolatileRepository -- put %s %d %d", key,
                      current_size_.load(), current_index_.load());
    return true;
  }
  /**
   *c
   * Deletes the key
   * @return status of the delete operation
   */
  virtual bool Delete(std::string key) {

    logger_->log_info("VolatileRepository -- delete %s", key);
    for (auto ent : value_vector_) {
      // let the destructor do the cleanup
      RepoValue value;
      if (ent->getValue(key, value)) {
        current_size_ -= value.size();
        return true;
      }

    }
    return false;
  }
  /**
   * Sets the value from the provided key. Once the item is retrieved
   * it may not be retrieved again.
   * @return status of the get operation.
   */
  virtual bool Get(std::string key, std::string &value) {
    for (auto ent : value_vector_) {
      // let the destructor do the cleanup
      RepoValue repo_value;

      if (ent->getValue(key, repo_value)) {
        current_size_ -= value.size();
        repo_value.emplace(value);
        logger_->log_info("VolatileRepository -- get %s %d", key,
                          current_size_.load());
        return true;
      }

    }
    return false;
  }

  void setConnectionMap(
      std::map<std::string, std::shared_ptr<minifi::Connection>> &connectionMap) {
    this->connectionMap = connectionMap;
  }
  void loadComponent();

  void start() {
    if (this->purge_period_ <= 0)
      return;
    if (running_)
      return;
    thread_ = std::thread(&VolatileRepository::run, shared_from_this());
    thread_.detach();
    running_ = true;
    logger_->log_info("%s Repository Monitor Thread Start", name_.c_str());
  }

 protected:

  /**
   * Tests whether or not the current size exceeds the capacity
   * if the new prospectiveSize is inserted.
   * @param prospectiveSize size of item to be added.
   */
  inline bool exceedsCapacity(uint32_t prospectiveSize) {
    if (current_size_ + prospectiveSize > max_size_)
      return true;
    else
      return false;
  }
  /**
   * Purges the volatile repository.
   */
  void purge();

 private:
  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;

  std::atomic<uint32_t> current_size_;
  std::atomic<uint16_t> current_index_;
  std::vector<AtomicEntry*> value_vector_;
  uint32_t max_count_;
  uint32_t max_size_;
  std::shared_ptr<logging::Logger> logger_;
}
;

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileRepository_H_ */
