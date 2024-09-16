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
#include "core/repository/VolatileRepository.h"
#include <map>
#include <memory>
#include <limits>
#include <string>
#include <vector>
#include "FlowFileRecord.h"

namespace org::apache::nifi::minifi::core::repository {

bool VolatileRepository::initialize(const std::shared_ptr<Configure> &configure) {
  repo_data_.initialize(configure, core::ThreadedRepositoryImpl::getName());

  logger_->log_info("Resizing value_vector for {} count is {}", core::ThreadedRepositoryImpl::getName(), repo_data_.max_count);
  logger_->log_info("Using a maximum size for {} of {}", core::ThreadedRepositoryImpl::getName(), repo_data_.max_size);
  return true;
}

bool VolatileRepository::Put(const std::string& key, const uint8_t *buf, size_t bufLen) {
  RepoValue<std::string> new_value(key, buf, bufLen);

  const size_t size = new_value.size();
  bool updated = false;
  size_t reclaimed_size = 0;
  RepoValue<std::string> old_value;
  do {
    uint32_t private_index = current_index_.fetch_add(1);
    // round robin through the beginning
    if (private_index >= repo_data_.max_count) {
      uint32_t new_index = private_index + 1;
      if (current_index_.compare_exchange_weak(new_index, 1)) {
        private_index = 0;
      } else {
        continue;
      }
    }

    updated = repo_data_.value_vector.at(private_index)->setRepoValue(new_value, old_value, reclaimed_size);
    logger_->log_debug("Set repo value at {} out of {} updated {} current_size {}, adding {} to  {}",
      private_index, repo_data_.max_count, updated, reclaimed_size, size, repo_data_.current_size.load());
    if (updated && reclaimed_size > 0) {
      emplace(old_value);
    }
    if (reclaimed_size > 0) {
      /**
       * this is okay since current_size is really an estimate.
       * we don't need precise counts.
       */
      if (repo_data_.current_size < reclaimed_size) {
        repo_data_.current_size = 0;
      } else {
        repo_data_.current_size -= reclaimed_size;
      }
    }
  } while (!updated);
  repo_data_.current_size += size;
  if (repo_data_.current_entry_count < repo_data_.max_count) {
    ++repo_data_.current_entry_count;
  }

  logger_->log_debug("VolatileRepository -- put {} {}", repo_data_.current_size.load(), current_index_.load());
  return true;
}

bool VolatileRepository::MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>& data) {
  for (const auto& item : data) {
    if (!Put(item.first, reinterpret_cast<const uint8_t*>(item.second->getBuffer().data()), item.second->size())) {
      return false;
    }
  }
  return true;
}

bool VolatileRepository::Delete(const std::string& key) {
  logger_->log_debug("Delete from volatile");
  for (auto ent : repo_data_.value_vector) {
    // let the destructor do the cleanup
    RepoValue<std::string> value;
    if (ent->getValue(key, value)) {
      repo_data_.current_size -= value.size();
      --repo_data_.current_entry_count;
      logger_->log_debug("Delete and pushed into purge_list from volatile");
      emplace(value);
      return true;
    }
  }
  return false;
}

bool VolatileRepository::Get(const std::string &key, std::string &value) {
  for (auto ent : repo_data_.value_vector) {
    RepoValue<std::string> repo_value;
    if (ent->getValue(key, repo_value)) {
      repo_value.emplace(value);
      return true;
    }
  }
  return false;
}

}  // namespace org::apache::nifi::minifi::core::repository
