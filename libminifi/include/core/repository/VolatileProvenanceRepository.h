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
#pragma once

#include <string>
#include <string_view>

#include "VolatileRepository.h"

namespace org::apache::nifi::minifi::core::repository {

class VolatileProvenanceRepository : public VolatileRepository {
 public:
  explicit VolatileProvenanceRepository(std::string_view repo_name = "",
                                        std::string /*dir*/ = REPOSITORY_DIRECTORY,
                                        std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                                        int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                                        std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
    : VolatileRepository(repo_name.length() > 0 ? repo_name : core::className<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod) {
  }

  ~VolatileProvenanceRepository() override {
    stop();
  }

 private:
  void run() override {
  }

  std::thread& getThread() override {
    return thread_;
  }

  void emplace(RepoValue<std::string> &old_value) override {
    purge_list_.push_back(old_value.getKey());
  }

  std::thread thread_;
};

}  // namespace org::apache::nifi::minifi::core::repository
