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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEREPOSITORY_H_

#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cinttypes>

#include "AtomicRepoEntries.h"
#include "Connection.h"
#include "core/Core.h"
#include "core/Repository.h"
#include "core/SerializableComponent.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using RocksDB as the primary substrate.
 */
template<typename KeyType, typename RepositoryType = core::Repository>
class VolatileRepository : public RepositoryType {
 public:
  static const char *volatile_repo_max_count;
  static const char *volatile_repo_max_bytes;

  explicit VolatileRepository(std::string repo_name = "",
                              std::string /*dir*/ = REPOSITORY_DIRECTORY,
                              std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                              int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                              std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        RepositoryType(repo_name.length() > 0 ? repo_name : core::getClassName<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod),
        current_size_(0),
        current_index_(0),
        max_count_(10000),
        max_size_(static_cast<size_t>(maxPartitionBytes * 0.75)),
        logger_(logging::LoggerFactory<VolatileRepository>::getLogger()) {
    purge_required_ = false;
  }

  ~VolatileRepository() override;

  /**
   * Initialize the volatile repository
   **/
  bool initialize(const std::shared_ptr<Configure> &configure) override;

  bool isNoop() const override {
    return false;
  }

  /**
   * Places a new object into the volatile memory area
   * @param key key to add to the repository
   * @param buf buffer
   **/
  bool Put(const KeyType& key, const uint8_t *buf, size_t bufLen) override;

  /**
   * Places new objects into the volatile memory area
   * @param data the key-value pairs to add to the repository
   **/
  bool MultiPut(const std::vector<std::pair<KeyType, std::unique_ptr<io::BufferStream>>>& data) override;

  /**
   * Deletes the key
   * @return status of the delete operation
   */
  bool Delete(const KeyType& key) override;

  /**
   * Sets the value from the provided key. Once the item is retrieved
   * it may not be retrieved again.
   * @return status of the get operation.
   */
  bool Get(const KeyType& key, std::string &value) override;
  /**
   * Deserializes objects into store
   * @param store vector in which we will store newly created objects.
   * @param max_size size of objects deserialized
   */
  bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size, std::function<std::shared_ptr<core::SerializableComponent>()> lambda) override;

  /**
   * Deserializes objects into a store that contains a fixed number of objects in which
   * we will deserialize from this repo
   * @param store precreated object vector
   * @param max_size size of objects deserialized
   */
  bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) override;

  /**
   * Function to load this component.
   */
  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override;

  uint64_t getRepoSize() const override {
    return current_size_;
  }

 protected:
  virtual void emplace(RepoValue<KeyType> &old_value) {
    std::lock_guard<std::mutex> lock(purge_mutex_);
    purge_list_.push_back(old_value.getKey());
  }

  // current size of the volatile repo.
  std::atomic<size_t> current_size_;
  // current index.
  std::atomic<uint32_t> current_index_;
  // value vector that exists for non blocking iteration over
  // objects that store data for this repo instance.
  std::vector<AtomicEntry<KeyType>*> value_vector_;

  // max count we are allowed to store.
  uint32_t max_count_;
  // maximum estimated size
  size_t max_size_;

  bool purge_required_;

  std::mutex purge_mutex_;
  // purge list
  std::vector<KeyType> purge_list_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

template<typename KeyType, typename RepositoryType>
const char *VolatileRepository<KeyType, RepositoryType>::volatile_repo_max_count = "max.count";
template<typename KeyType, typename RepositoryType>
const char *VolatileRepository<KeyType, RepositoryType>::volatile_repo_max_bytes = "max.bytes";

template<typename KeyType, typename RepositoryType>
void VolatileRepository<KeyType, RepositoryType>::loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) {
}

// Destructor
template<typename KeyType, typename RepositoryType>
VolatileRepository<KeyType, RepositoryType>::~VolatileRepository() {
  for (auto ent : value_vector_) {
    delete ent;
  }
}

/**
 * Initialize the volatile repository
 **/
template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::initialize(const std::shared_ptr<Configure> &configure) {
  std::string value = "";

  if (configure != nullptr) {
    int64_t max_cnt = 0;
    std::stringstream strstream;
    strstream << Configure::nifi_volatile_repository_options << RepositoryType::getName() << "." << volatile_repo_max_count;
    if (configure->get(strstream.str(), value)) {
      if (core::Property::StringToInt(value, max_cnt)) {
        max_count_ = gsl::narrow<uint32_t>(max_cnt);
      }
    }

    strstream.str("");
    strstream.clear();
    int64_t max_bytes = 0;
    strstream << Configure::nifi_volatile_repository_options << RepositoryType::getName() << "." << volatile_repo_max_bytes;
    if (configure->get(strstream.str(), value)) {
      if (core::Property::StringToInt(value, max_bytes)) {
        if (max_bytes <= 0) {
          max_size_ = std::numeric_limits<uint32_t>::max();
        } else {
          max_size_ = gsl::narrow<size_t>(max_bytes);
        }
      }
    }
  }

  logging::LOG_INFO(logger_) << "Resizing value_vector_ for " << RepositoryType::getName() << " count is " << max_count_;
  logging::LOG_INFO(logger_) << "Using a maximum size for " << RepositoryType::getName() << " of  " << max_size_;
  value_vector_.reserve(max_count_);
  for (uint32_t i = 0; i < max_count_; i++) {
    value_vector_.emplace_back(new AtomicEntry<KeyType>(&current_size_, &max_size_));
  }
  return true;
}

/**
 * Places a new object into the volatile memory area
 * @param key key to add to the repository
 * @param buf buffer
 **/
template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::Put(const KeyType& key, const uint8_t *buf, size_t bufLen) {
  RepoValue<KeyType> new_value(key, buf, bufLen);

  const size_t size = new_value.size();
  bool updated = false;
  size_t reclaimed_size = 0;
  RepoValue<KeyType> old_value;
  do {
    uint32_t private_index = current_index_.fetch_add(1);
    // round robin through the beginning
    if (private_index >= max_count_) {
      uint32_t new_index = private_index + 1;
      if (current_index_.compare_exchange_weak(new_index, 1)) {
        private_index = 0;
      } else {
        continue;
      }
    }

    updated = value_vector_.at(private_index)->setRepoValue(new_value, old_value, reclaimed_size);
    logger_->log_debug("Set repo value at %u out of %u updated %u current_size %u, adding %u to  %u", private_index, max_count_, updated == true, reclaimed_size, size, current_size_.load());
    if (updated && reclaimed_size > 0) {
      emplace(old_value);
    }
    if (reclaimed_size > 0) {
      /**
       * this is okay since current_size_ is really an estimate.
       * we don't need precise counts.
       */
      if (current_size_ < reclaimed_size) {
        current_size_ = 0;
      } else {
        current_size_ -= reclaimed_size;
      }
    }
  } while (!updated);
  current_size_ += size;

  logger_->log_debug("VolatileRepository -- put %zu %" PRIu32, current_size_.load(), current_index_.load());
  return true;
}

template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::MultiPut(const std::vector<std::pair<KeyType, std::unique_ptr<io::BufferStream>>>& data) {
  for (const auto& item : data) {
    if (!Put(item.first, item.second->getBuffer().template as_span<const uint8_t>().data(), item.second->size())) {
      return false;
    }
  }
  return true;
}

/**
 * Deletes the key
 * @return status of the delete operation
 */
template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::Delete(const KeyType& key) {
  logger_->log_debug("Delete from volatile");
  for (auto ent : value_vector_) {
    // let the destructor do the cleanup
    RepoValue<KeyType> value;
    if (ent->getValue(key, value)) {
      current_size_ -= value.size();
      logger_->log_debug("Delete and pushed into purge_list from volatile");
      emplace(value);
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
template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::Get(const KeyType &key, std::string &value) {
  for (auto ent : value_vector_) {
    // let the destructor do the cleanup
    RepoValue<KeyType> repo_value;
    if (ent->getValue(key, repo_value)) {
      current_size_ -= value.size();
      repo_value.emplace(value);
      return true;
    }
  }
  return false;
}

template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store,
                                                      size_t &max_size, std::function<std::shared_ptr<core::SerializableComponent>()> lambda) {
  size_t requested_batch = max_size;
  max_size = 0;
  for (auto ent : value_vector_) {
    // let the destructor do the cleanup
    RepoValue<KeyType> repo_value;

    if (ent->getValue(repo_value)) {
      std::shared_ptr<core::SerializableComponent> newComponent = lambda();
      // we've taken ownership of this repo value
      newComponent->DeSerialize(repo_value.getBuffer());

      store.push_back(newComponent);

      current_size_ -= repo_value.getBuffer().size();

      if (max_size++ >= requested_batch) {
        break;
      }
    }
  }
  if (max_size > 0) {
    return true;
  } else {
    return false;
  }
}

template<typename KeyType, typename RepositoryType>
bool VolatileRepository<KeyType, RepositoryType>::DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) {
  logger_->log_debug("VolatileRepository -- DeSerialize %u", current_size_.load());
  max_size = 0;
  for (auto ent : value_vector_) {
    // let the destructor do the cleanup
    RepoValue<KeyType> repo_value;

    if (ent->getValue(repo_value)) {
      // we've taken ownership of this repo value
      store.at(max_size)->DeSerialize(repo_value.getBuffer());
      current_size_ -= repo_value.getBuffer().size();
      if (max_size++ >= store.size()) {
        break;
      }
    }
  }
  if (max_size > 0) {
    return true;
  } else {
    return false;
  }
}

}  // namespace repository
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEREPOSITORY_H_
