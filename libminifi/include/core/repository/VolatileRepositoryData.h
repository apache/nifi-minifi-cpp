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

#include <vector>
#include <atomic>
#include <string>
#include <memory>

#include "AtomicRepoEntries.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::core::repository {

static constexpr const char *VOLATILE_REPO_MAX_COUNT = "max.count";
static constexpr const char *VOLATILE_REPO_MAX_BYTES = "max.bytes";

struct VolatileRepositoryData {
  VolatileRepositoryData(uint32_t max_count, size_t max_size);
  ~VolatileRepositoryData();

  void initialize(const std::shared_ptr<Configure> &configure, const std::string& repo_name);
  void clear();

  uint64_t getRepositorySize() const {
    return current_size;
  }

  uint64_t getMaxRepositorySize() const {
    return max_size;
  }

  uint64_t getRepositoryEntryCount() const {
    return current_entry_count;
  }

  bool isFull() const {
    return current_size >= max_size;
  }

  // current size of the volatile repo.
  std::atomic<size_t> current_size;
  std::atomic<size_t> current_entry_count;
  // value vector that exists for non blocking iteration over
  // objects that store data for this repo instance.
  std::vector<AtomicEntry<std::string>*> value_vector;
  // max count we are allowed to store.
  uint32_t max_count;
  // maximum estimated size
  size_t max_size;
};

}  // namespace org::apache::nifi::minifi::core::repository
